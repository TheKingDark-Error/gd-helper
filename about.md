# GD Helper

A feature-packed Geometry Dash mod built with Geode SDK.

## Features

- **FPS Counter**: Real-time FPS display in the top-left corner during gameplay.
- **Auto Play Bot**: An advanced heuristic bot that attempts to play levels automatically. 
  **Important**: When Auto Play is active, level completion progress is NOT saved.
  **Note**: This bot works best on Easy-Hard levels. Insane and Demon levels require frame-perfect precision that heuristic bots cannot reliably achieve.
- **Click Sounds**: Plays distinct sounds on click and release. Works for both manual play and bot play.
- **In-Game Settings**: Toggle all features directly from the Pause Menu — no need to exit the level!

## Supported Game Modes (Bot)

- Cube: Basic jump over hazards and orbs
- Ship: Hold to fly up, release to fall (simple obstacle avoidance)
- Wave: Zigzag through gaps (basic)
- UFO: Auto-click on jump orbs

## How to Build (GitHub Actions - No PC needed!)

1. Fork this repository on GitHub
2. Go to Actions tab → Enable workflows
3. Push any commit to trigger the build
4. Download the `.geode` artifact from the latest workflow run
5. Install on your phone via Geode launcher

## How to Build (Local)

1. Install [Geode CLI](https://docs.geode-sdk.org/getting-started/geode-cli)
2. Set `GEODE_SDK` environment variable
3. Run `geode build` (or `geode build -p android64` for Android)
4. Install the resulting `.geode` file
