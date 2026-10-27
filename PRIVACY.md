# Privacy

This document describes the current privacy posture for Harken Search public-release candidates.

## Local data Harken Search can process

- Audio library paths, filenames, and metadata
- User-entered prompts and workflow settings
- Generated cover output paths and related local asset files
- Whisper model files downloaded into the local `models/` directory

## Third-party / networked AI surfaces

- Whisper model downloads currently use Hugging Face URLs from `src/AIEngine.cpp`
- Cover generation is currently offline and does not send prompts to a third-party image API

## Public-release rules

- Public release builds must not inline API keys into frontend assets or Vite bundles.
- Cloud AI features should only send user data after explicit user action.
- If secure secret storage and clear opt-in UX are not complete, cloud AI features should remain disabled, hidden, or excluded from the public-release package.
- The retired React/WebUI prototype is not shipped. The remaining WebView2 startup/login surfaces are implemented by the compiled native application and do not load that prototype bundle.
