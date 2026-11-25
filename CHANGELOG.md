# Changelog

All notable changes to the project. Format based on
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow
[SemVer](https://semver.org/). Until 3.5, the game was called Splendor.

## [Unreleased]

### Added
- **Season pass** (free for everyone): 50 tiers per season, a reward every
  2 tiers. Earn XP with online games (more for a win), a bonus for your first
  game of the day and 3 weekly challenges.
- **Cosmetic rewards**, new each season: board themes, card backs, token
  skins, profile frames, titles, avatars and chat reactions. No gameplay
  advantage.
- **Appearance** screen to choose what you wear. Frames, titles and avatars
  are seen by the other players (lobbies, games, Discord `/profile`).

## [4.1.0] - 2026-11-25

### Added
- **Automatic seasons**: a season lasts two months and changes by itself on
  January 1, March 1, May 1, July 1, September 1 and November 1 at midnight
  (Paris time). The end of each season is announced on Discord.
- **Two Elo ratings**: the **global Elo** (never reset) and the **season Elo**
  (back to 1000 at the start of each season).
- **Two matchmaking queues**: **Ranked** (matched on the global Elo, changes
  it) and **Season** (matched on the season Elo, changes it).
- **Season leaderboard**: at least 10 season games to be ranked; the top 10
  earn a badge. The leaderboard (game, site, Discord) shows the season and the
  global ranking.
- **Challenge a friend**: a "Challenge" button in the friends list creates a
  friendly 2-player lobby; your friend gets a notification with the code.
- **Website**: better search engine and link previews (page titles and
  descriptions, English and French addresses, share image, sitemap).

### Changed
- Games in a private lobby (code, Discord voice channel, Activity,
  `/challenge`) are now **friendly**: no Elo change.
- The end of a season no longer brings the global Elo halfway back to 1000.

## [4.0.0] - 2026-11-25

### Changed
- **License**: Gemspire is now proprietary (© 2026 Gemspire, all rights
  reserved), free to download and play. Versions before 4.0 stay under GPL-3.0.

### Added
- **Rules** button on the home screen: the rules in short, in two columns.
- **Board themes**: Green felt, Night, Wood, High contrast and Light (Theme
  button on the home screen, remembered in `theme.txt`).
- **Sharper display**: the browser version draws at the real screen size
  (Retina screens included) instead of stretching a 1280 × 800 picture; on
  Windows the window follows the display scaling. **F11** toggles full screen.
- **Link Google and Discord** to the same account (online menu): sign in with
  either, same username and Elo.
- **Admin panel** at `/panel-admin` (Discord sign-in, administrators only):
  server state, close sign-ups, pause new games, in-game announcement, account
  search and management (rename, Elo, season badges, sign out everywhere,
  ban, delete, cancel a game), games-per-day chart, end a stuck game,
  minimum game version without restarting, `/report` reports, admin log.
- Bot **`/dev`** commands for the administrators: `status`, `signups`,
  `games`, `announce`, `rename`, `elo`, `badge`, `remove-badge`, `logout`,
  `end-game`, `min-version`.
- **Site icon** (browser tab, bookmarks, phone home screen).
- **SHA-256 checksums** (`SHA256SUMS`) published with every release, to check
  that a download is intact.

### Security
- Independent security test of the server, the site and the game; every
  finding is fixed:
  - signing in now uses a secret kept by the game (never in a browser
    address), so a leaked sign-in link can no longer be used to take over an
    account;
  - moderation actions from the bot record which moderator did them;
  - HTTPS enforced by the browser (HSTS), request rate limits on sign-in, the
    Discord Activity only talks to Discord, safer release workflows.
- Games older than 4.0.0 must be updated to sign in (the game shows the update
  screen).

### Fixed
- The Discord Activity could open on a blank page.

## [3.9.0] - 2026-11-23

### Added
- **Spectator mode**: « Live games » in the online menu lists the games in
  progress; « Watch » opens one in read-only mode (no hidden information).
  Friends in a game have a « Watch » button too.
- **Replays**: every online game is recorded (seed and actions). « My games »
  lists your last 15 games; « Replay » plays one back step by step (arrows,
  Space for play / pause). Also available from the results screen.
- **Ranked seasons**: the leaderboard shows the current season. Moderators end
  it with `/mod new-season` (with confirmation): the top 50 is archived, the
  top 10 earn a badge (shown in the game and in `/profile`), and every Elo moves
  halfway back to 1000. The end of the season is announced in the leaderboard
  channel.
- **Find an opponent**: automatic 2-player matchmaking by Elo (same mode; the
  accepted gap starts at 100 and widens while you wait).
- Bot `/profile` shows achievements and season badges.

### Fixed
- A spectator could see the order of the decks.

## [3.8.0] - 2026-11-23

### Added
- **In-game chat** (online games): « Chat » button with the number of unread
  messages, 120 characters per message, 6 quick reactions (GG, Good luck!...),
  mute a player, anti-spam (1 message per second, 6 per 20 seconds). The new
  message shows for a few seconds at the bottom of the board. Messages are
  only kept in memory with the game.
- **Friends**: add a player by username, accept or decline requests, see
  whether your friends are online or in a game (« Friends » in the online
  menu). A request shows up in the friend's game.
- **Achievements**: 11 achievements (first victory, royal visit, no safety
  net, giant slayer...), unlocked at the end of online games, listed in the
  online menu and shown on the results screen.
- **Discord Activity**: everyone who opens Gemspire in the same voice channel
  lands in the same lobby (4 seats).
- Bot: **`/challenge @member`** creates a 2-player lobby and pings the
  challenged member (both games are told); **rank roles** by Elo (Pearl, Ruby,
  Sapphire, Emerald, Diamond) updated every 10 minutes (`RANK_ROLES=0` to
  turn them off).

### Fixed
- The leaderboard screen showed « Joueur » in English.
- Unstable server test (a tied game has no Elo change).

## [3.7.0] - 2026-11-23

### Added
- **The game speaks English and French**: window, browser version, console,
  game log, rules and server messages. The language follows the system (or
  the browser) on first launch; a « Français » / « English » button on the
  home screen switches it (kept in `langue.txt`). Console: French when
  `LANG` starts with `fr`.
- **Discord Activity**: the browser version can run inside a Discord voice
  channel and signs the player in with their Discord account
  (`POST /auth/discord/activity`). Needs the URL mappings set in the Discord
  developer portal (see `server/README.md`).
- Console: English commands (`t` take, `b` buy, `r 3 d` reserve from the
  deck) and options (`--quick`, `--classic`, `--load`); the French ones still
  work.

### Changed
- Online games: the server sends the game log in both languages (`log`,
  `log_fr`); error messages from the server and the bot routes are in
  English (translated by the game).
- Discord lobby announcements are in English.

## [3.6.0] - 2026-11-22

### Changed
- **The game is now called Gemspire**: title, website
  (https://gemspire.epsilon-forge.fr, the old address redirects), bot,
  executables (`gemspire-windows.exe`, `gemspire-linux`...), GitHub repository
  `Pierre-Pln-Bys/Gemspire`. Server settings and services keep their
  technical names (`SPLENDOR_*`, `splendor-server`).
- **GPL-3.0 license**.

### Added
- **Changelog on Discord**: every release posts its changes to a Discord
  channel (`tools/discord_release.py`, GitHub secret
  `DISCORD_RELEASE_WEBHOOK`).
- Bot: channels and roles can be set by **ID** (`CREATE_CHANNEL_ID`,
  `RANKING_CHANNEL_ID`, `MOD_LOG_CHANNEL_ID`, `MOD_ROLE_ID`, `PLAYER_ROLE_ID`);
  names remain the default.

## [3.5.0] - 2026-11-22

### Added
- **Play in your browser**, on computer and phone:
  https://gemspire.epsilon-forge.fr/play/. Same C++ code compiled to
  WebAssembly (Emscripten, `make web`): networking through `fetch`, text input
  through the browser dialog (phone keyboard), session and save kept in the
  browser, Discord / Google sign-in in the same tab (automatic return to the
  game). « Play in your browser » button on the website.
- **Website in English and French** (browser language or `?lang=`),
  addresses `/leaderboard`, `/terms`, `/privacy` (the French ones still work).
- **Bot in English**, translated into French for members whose Discord is
  in French: `/leaderboard`, `/profile`, `/link`, `/play`, `/rules`,
  `/report`, `/mod ban|unban|history|cancel-game|delete-account`.
- Release: web archive installed by `deploy/install.sh`. CI: web build.

### Fixed
- The remaining turn time was hidden under the noble tiles.

## [3.4.1] - 2026-11-22

### Added
- **Required version**: the game sends its version to the server; a version
  that is too old (or missing) is refused and the game shows the blocking
  « Update required » screen (download button, current local game saved).
  Minimum version: `SPLENDOR_MIN_VERSION`, by default the server's own.
  Versions up to 3.4.0 can no longer play online once the server is updated.

## [3.4.0] - 2026-11-22

### Added
- **Discord bot, the game's hub**:
  - `#leaderboard` channel: top 10 updated every 10 minutes;
  - commands to see the leaderboard and profiles, link an account (Player
    role), get the download links and rules, report a player;
  - moderation: `/mod ban` (game account suspension, 0 days = permanent),
    `/mod unban`, `/mod history` (alert on multiple accounts),
    `/mod cancel-game`, `/mod delete-account`; log in `#moderation`.
- Server: suspended accounts (sign-in refused, sessions closed, games
  forfeited, removed from the leaderboard), game history, game cancellation,
  account deletion, routes `/bot/profile`, `/bot/history`, `/bot/ban`,
  `/bot/unban`, `/bot/cancel-game`, `/bot/delete-account`. Database migrated
  automatically.
- « Discord server » wiki page: recommended roles, channels and settings.

### Changed
- The bot also keeps the leaderboard message ID in its state file (the old
  format is still read).
- Bot invite link: `applications.commands` scope and « manage roles »,
  « embed links », « read message history » permissions.

## [3.3.0] - 2026-11-22

### Added
- **Online turn time**: 2 minutes per turn (countdown shown). After that the
  turn passes; after 3 missed turns in a row the player forfeits. **Forfeit**
  button (two clicks). A player who forfeits is ranked last and loses Elo;
  the game goes on between the others. Before, closing the game blocked the
  game and avoided the loss.
- **Website**: home page and downloads, public **leaderboard**, **terms of
  use** and **privacy policy**. HTML without JavaScript, readable on phones.
- Logic: `Game::skip_turn`, `Game::forfeit`; saves keep forfeits (optional
  `forfeited` line).

### Fixed
- Token selection: forbidden combinations can no longer be picked (2 of the
  same colour from a stack of less than 4, more than 3 colours).
- Clearly visible announcements when a noble visits a player and when the
  last round starts.
- The server forgets games finished more than an hour ago (memory).
- Unstable server test (level I deck sometimes empty at the end of a game).

## [3.2.2] - 2026-11-22

### Fixed
- Windows: the graphical version no longer built (MSYS2's recent OpenSSL
  needs the `bcrypt` library). 3.2.1 was therefore not published; 3.2.2
  contains all its changes.

### Added
- Discord invite cover image (`assets/invite-cover.png`).

## [3.2.1] - 2026-11-22 (not published)

First version connected to the official server, after real-world testing.

### Changed
- The game connects to the official server by default (Raspberry Pi behind a
  Cloudflare tunnel). `serveur.txt` or `SPLENDOR_SERVER` point to another
  server.
- Rich Presence: game logo and clearer texts (username and Elo).
- Bot: an empty temporary channel is deleted after 30 seconds; the channel
  list is kept in a file so it survives a restart.
- Deployment: port 8090 (8080 is often taken), public IDs and project
  address prefilled.

### Fixed
- The game crashed at startup when Discord was open (Discord's
  `"evt": null` reply).
- « Confirm » (username), « Create lobby », « Join » and « Start game »
  stayed greyed out once signed in (long polls counted as pending requests).
- Server: a failed Discord webhook is logged.
- `install.sh` waits for the server to answer before checking `/health`.

## [3.2.0] - 2026-11-20

### Added
- **Discord Rich Presence**: the profile shows the menu, the game (mode,
  round, points) or the lobby (seats); « Join » from Discord opens the lobby
  in the game. Uses Discord's local IPC (Linux and Windows), no SDK.
- **Invites**: « Copy an invite » button in the lobby.
- **« Create a game » Discord bot** (Python, discord.py): temporary voice
  channel, game offer in the game if it is open, channel limited to the
  number of seats, deleted when empty.
- Server: player presence, game notifications, `/config`, bot API protected
  by a key, lobbies linked to a voice channel, new lobbies announced through
  a Discord webhook.
- **Deployment** on the Raspberry Pi: `deploy/install.sh`, hardened systemd
  services, settings template.
- Tests: local fake Discord (`make test-discord`), bot and integration with
  the server (`make test-bot`).

## [3.1.0] - 2026-11-20

### Added
- **Online play**: sign in with Discord or Google (no password), unique
  username, lobbies with a code, games refereed by the server, **Elo**
  ranking and leaderboard.
- `splendor-server` (C++: cpp-httplib, SQLite, OpenSSL) reusing the game
  logic; each player only receives what they are allowed to see (decks and
  other players' reserved cards hidden); session tokens stored as SHA-256
  fingerprints.
- Sign-in, username, online menu, lobby and leaderboard screens; results
  with Elo change.
- `make test-server`: two players play a full online game (invalid moves
  refused, Elo, leaderboard, hidden information).

### Changed
- Common move format (`Move`) for the window, the AI and the server; the AI
  picks a move (`ai_choose`) without playing it.
- The Linux graphical version uses the system's OpenSSL (libssl3).

## [3.0.0] - 2026-11-20

### Added
- New graphical interface with **raylib**, shipped as a single executable
  for Windows and Linux, nothing to install.
- Home and new game screens drawn in the game (name, colour and type of
  each player).
- Game log (`J`), « missing ... » tooltips over cards and nobles, keyboard
  shortcuts (1 to 5, Enter, R, Escape, Ctrl+S).
- Hand-over screen between two human players in classic games.
- A symbol for each gem (colour blindness).
- Animations: sliding tokens, cards flying to the player, card flipping when
  it arrives on the board.
- Embedded DejaVu Sans font (accents); « Menu » button that saves the
  current game.

### Changed
- `enum class Color` becomes `enum class Gem` (raylib already has a `Color`
  type).
- Releases: the graphical versions are now the main executables; the console
  versions are called `*-console-*`.

### Removed
- GTK 3 interface (replaced by raylib) and the GTK dependency.

## [2.4.0] - 2026-11-20

### Added
- Save and resume a game (`.splendor` text files): `s` command and
  `--charger` option in the console, « Save » / « Open… » buttons
  (Ctrl+S / Ctrl+O) in the window.
- Tests: a reloaded game ends exactly like the original; invalid files are
  refused cleanly.

## [2.3.0] - 2026-11-20

### Added
- « Easy » and « medium » computer opponents, in the console and in the
  window (« Played by » column; the computer plays after a short pause).
- 1,000-game tournament between computers in the tests: no game crashes or
  loses a token, and the medium AI beats the easy one.

## [2.2.0] - 2026-11-20

### Added
- CI: build and tests on every push and pull request, on Linux and Windows.
- Action history: every action (taking tokens, buying, reserving,
  discarding, noble visit, passing) is described in the console and in the
  window's status line.
- Console: each player has a colour (name, player table, results).
- Turn flow tests (discard with gold, choice between two nobles, last round,
  chained passes, blocked game) and 300 simulated full games.

### Changed
- The turn flow (discard, nobles, next player, passes) is handled by `Game`
  itself; the console and the window share this code.
- Updated GitHub Actions (no more Node.js 20 warning).

## [2.1.0] - 2026-11-20

### Changed
- The 5th resource colour is now called Black (instead of Maroon), in the
  console and in the graphical interface.
- Graphical interface: affordable cards outlined in gold, selection in
  white.

### Added
- Graphical interface: each player picks a colour when creating the game;
  their panel is outlined with it during their turn and their reserved cards
  use it as border.
- Release workflow: can be started manually to publish an existing tag
  (Actions tab or `gh workflow run release.yml -f tag=vX.Y.Z`).

## [2.0.0] - 2026-11-20

### Added
- `splendor-gui` graphical interface (GTK 3 + Cairo): board drawn like a real
  table, played entirely with the mouse, new game dialog, end screen with
  « Play again ».
- GitHub Actions workflow: for every major version (`vX.0.0` tag), the
  Windows (.exe) and Linux executables are attached to the release.
- Console version: colours and accents on Windows, waits for Enter at the
  end of the game so the window does not close.

## [1.0.0] - 2026-11-20

### Added
- Complete game in C++17, in the terminal.
- Two modes: quick game (no gold, no reservations) and classic game (gold
  tokens, up to 3 reserved cards).
- Board drawn like a real table: noble tiles, card rows with their deck,
  token bank on the side, player table.
- `--rapide`, `--classique`, `--version` options and game seed.
- Logic tests (`make test`).
- The game stops if no player can play anymore.

### Removed
- Old C99 + ncurses base (ncurses wrapper, examples, splendor.h).
