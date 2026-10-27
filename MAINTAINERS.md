# Aris Maintenance Roles

This file defines durable responsibilities, not individual assignments. Actual
people and GitHub teams are assigned by repository administrators through
repository access, review rules, and release configuration.

## Repository Maintainers

Repository Maintainers protect architectural coherence, review cross-domain
changes, keep required checks healthy, and decide whether a change is ready to
merge. They enforce the workflow in [CONTRIBUTING.md](CONTRIBUTING.md).

## Module ownership

Each change names one primary domain owner and any affected secondary domains.

| Domain | Responsibility |
| --- | --- |
| Database | Database lifecycle, schemas, migrations, caches, and SQL |
| Search | Query syntax, recall, filters, and search indexing semantics |
| Audio | Decode, playback, waveform, and audio metadata extraction |
| Indexing | Scan, filesystem observation, queues, and index lifecycle |
| UI | Visible state, interaction, accessibility, and user workflows |
| Performance | Budgets, main-thread work, memory, and large-library behavior |

Module owners review behavior and evidence in their domain. Cross-domain work
requires review from each affected domain; high-risk changes follow the expanded
review rules in the authoritative workflow.

## Release Manager

The Release Manager owns release-branch freeze, release evidence, artifact
verification, tags, and release notes. Public contents must stay inside the
publication set described by this repository's contributing and release notes.

## Security Response

The Security Response role privately receives, triages, coordinates, and
controls disclosure of vulnerability reports according to [SECURITY.md](SECURITY.md).

## Repository-admin configuration

Repository administrators assign real people or teams to these roles, configure
`main` branch protection, require pull request review and green checks, and set
release permissions. A `CODEOWNERS` file must be created only after administrators
approve real GitHub teams or handles; no synthetic ownership identity is tracked
in this repository.
