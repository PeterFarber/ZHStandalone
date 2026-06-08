# ZH Standalone

4v4 server-authoritative hockey prototype (2D first). Listen-server multiplayer over UDP (ENet), rendered with raylib.

## Layout

| Folder | What it is |
|--------|------------|
| `game/` | C++ game — source, assets, maps, Premake build |
| `map-editor/` | Browser map editor (TypeScript / Vite) |
| `docs/` | [Developer guides](docs/README.md) |

## Quick start

**Game (Windows):**

```bat
game\scripts\build.bat
game\scripts\run.bat
```

**Map editor:**

```bat
cd map-editor
npm install
npm run dev
```

Export maps to `game/maps/default.json`, then rebuild the game.

Full setup (Linux, troubleshooting, clangd): **[docs/getting-started.md](docs/getting-started.md)**

## Documentation

New to the codebase? Read **[docs/README.md](docs/README.md)** → getting started → project overview → code tour.

## License

MIT — see [LICENSE](LICENSE). Dependency notes: [docs/third-party.md](docs/third-party.md).
