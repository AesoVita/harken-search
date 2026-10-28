"""A card clone can be removed only after its work is on the QA branch."""
from pathlib import Path
import os
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from tools import remove_card_directory as cleanup


REPOSITORY = "xindong/harken-search"
REF = "qa/current"


def git(repo, *args):
    return subprocess.run(["git", "-C", str(repo), *args], check=True, capture_output=True, text=True).stdout.strip()


class CardDirectoryCleanupTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.origin = self.root / "origin.git"
        subprocess.run(["git", "init", "--bare", "-b", REF, str(self.origin)], check=True, capture_output=True)
        seed = self.root / "seed"
        subprocess.run(["git", "clone", str(self.origin), str(seed)], check=True, capture_output=True)
        git(seed, "config", "user.name", "CI")
        git(seed, "config", "user.email", "ci@example.invalid")
        (seed / "tracked.txt").write_text("first\n", encoding="utf-8")
        git(seed, "add", "tracked.txt")
        git(seed, "commit", "-m", "first")
        git(seed, "push", "origin", "HEAD:" + REF)
        self.card = self.root / "card-one"
        self.main_repo = self.root / "main-repository"
        for target in (self.card, self.main_repo):
            subprocess.run(["git", "clone", str(self.origin), str(target)], check=True, capture_output=True)
        git(self.card, "config", "user.name", "CI")
        git(self.card, "config", "user.email", "ci@example.invalid")
        git(self.card, "remote", "set-url", "origin", "https://github.com/" + REPOSITORY + ".git")
        actual_git = cleanup.git

        def local_fetch(repo, *args):
            if repo == self.card and args[:3] == ("fetch", "--no-tags", "origin"):
                return actual_git(repo, "fetch", "--no-tags", str(self.origin), *args[3:])
            return actual_git(repo, *args)

        fetch_patch = patch.object(cleanup, "git", side_effect=local_fetch)
        fetch_patch.start()
        self.addCleanup(fetch_patch.stop)

    def args(self, *mode):
        return [
            "--workspace-root", str(self.root), "--card-name", self.card.name,
            "--card-prefix", "card-", "--main-repository", str(self.main_repo),
            "--repository", REPOSITORY, "--integration-ref", REF, *mode,
        ]

    def refused(self, *mode):
        with self.assertRaises(cleanup.CleanupRefused):
            cleanup.main(self.args(*mode))
        self.assertTrue((self.card / ".git").exists())

    def test_clean_merged_clone_needs_dry_run_and_exact_head_to_delete(self):
        head = git(self.card, "rev-parse", "HEAD")
        self.assertEqual(cleanup.main(self.args("--dry-run")), 0)
        self.assertTrue(self.card.exists())
        self.refused("--execute")
        self.refused("--execute", "--expected-head", "0" * 40)
        self.assertEqual(cleanup.main(self.args("--execute", "--expected-head", head)), 0)
        self.assertFalse(self.card.exists())
        self.assertTrue(self.main_repo.exists())
        self.assertTrue(self.origin.exists())

    def test_dirty_file_and_stash_are_preserved(self):
        (self.card / "untracked.txt").write_text("work", encoding="utf-8")
        self.refused("--dry-run")
        git(self.card, "stash", "push", "--include-untracked", "-m", "saved")
        self.refused("--dry-run")

    def test_hidden_branch_with_unique_commit_is_preserved(self):
        git(self.card, "switch", "-c", "backup/unfinished")
        (self.card / "tracked.txt").write_text("new\n", encoding="utf-8")
        git(self.card, "commit", "-am", "unfinished")
        git(self.card, "switch", REF)
        self.refused("--dry-run")

    def test_current_head_with_unique_commit_is_preserved(self):
        (self.card / "tracked.txt").write_text("not merged", encoding="utf-8")
        git(self.card, "commit", "-am", "not merged")
        self.refused("--dry-run")

    def test_dry_run_fetches_the_latest_integration_ref(self):
        old_ref = git(self.card, "rev-parse", "origin/" + REF)
        seed = self.root / "seed"
        (seed / "tracked.txt").write_text("accepted later", encoding="utf-8")
        git(seed, "commit", "-am", "accepted later")
        git(seed, "push", "origin", "HEAD:" + REF)
        self.assertEqual(cleanup.main(self.args("--dry-run")), 0)
        self.assertNotEqual(git(self.card, "rev-parse", "origin/" + REF), old_ref)
        self.assertEqual(git(self.card, "rev-parse", "origin/" + REF), git(seed, "rev-parse", "HEAD"))

    def test_wrong_remote_or_missing_qa_ref_is_preserved(self):
        git(self.card, "remote", "set-url", "origin", str(self.origin))
        self.refused("--dry-run")
        git(self.card, "remote", "set-url", "origin", "https://github.com/" + REPOSITORY + ".git")
        self.refused("--integration-ref", "qa/missing", "--dry-run")

    def test_url_rewrite_cannot_redirect_valid_looking_origin(self):
        git(self.card, "config", "url." + self.origin.as_uri() + ".insteadOf", "https://github.com/" + REPOSITORY + ".git")
        self.refused("--dry-run")

    def test_ssh_alias_must_resolve_to_github(self):
        git(self.card, "remote", "set-url", "origin", "git@github-test-alias:" + REPOSITORY + ".git")
        with patch.object(cleanup, "ssh_hostname", return_value="github.com"):
            self.assertEqual(cleanup.main(self.args("--dry-run")), 0)
        with patch.object(cleanup, "ssh_hostname", return_value="other.example"):
            self.refused("--dry-run")

    def test_path_escape_main_repo_and_existing_target_lock_are_refused(self):
        with self.assertRaises(cleanup.CleanupRefused):
            cleanup.main(self.args("--dry-run") + ["--card-name", "../seed"])
        with self.assertRaises(cleanup.CleanupRefused):
            cleanup.main(self.args("--dry-run") + ["--card-name", "card-missing"])
        with self.assertRaises(cleanup.CleanupRefused):
            cleanup.main(self.args("--dry-run") + ["--main-repository", str(self.card)])
        lock = self.root / ".card-one.cleanup.lock"
        lock.write_text("another owner", encoding="utf-8")
        self.refused("--dry-run")
        self.assertEqual(lock.read_text(encoding="utf-8"), "another owner")

    def test_linked_card_or_nested_link_is_refused(self):
        link = self.root / "card-linked"
        try:
            os.symlink(self.card, link, target_is_directory=True)
        except OSError:
            link.mkdir()
            real_check = cleanup.is_reparse
            with patch.object(cleanup, "is_reparse", side_effect=lambda path: Path(path) == link or real_check(path)):
                with self.assertRaisesRegex(cleanup.CleanupRefused, "链接|重解析"):
                    cleanup.main(self.args("--dry-run") + ["--card-name", link.name])
        else:
            with self.assertRaisesRegex(cleanup.CleanupRefused, "链接|重解析"):
                cleanup.main(self.args("--dry-run") + ["--card-name", link.name])
        nested = self.card / "linked-file"
        try:
            os.symlink(self.main_repo / "tracked.txt", nested)
        except OSError:
            nested.write_text("simulated link", encoding="utf-8")
            real_check = cleanup.is_reparse
            with patch.object(cleanup, "is_reparse", side_effect=lambda path: Path(path) == nested or real_check(path)):
                with self.assertRaisesRegex(cleanup.CleanupRefused, "链接|重解析"):
                    cleanup.main(self.args("--dry-run"))
        else:
            with self.assertRaisesRegex(cleanup.CleanupRefused, "链接|重解析"):
                cleanup.main(self.args("--dry-run"))

    def test_read_failure_and_unsafe_repository_argument_refuse_before_delete(self):
        git(self.card, "config", "--unset", "remote.origin.url")
        self.refused("--dry-run")
        with self.assertRaises(cleanup.CleanupRefused):
            cleanup.main(self.args("--dry-run") + ["--repository", "owner/repo;echo danger"])


if __name__ == "__main__":
    unittest.main()
