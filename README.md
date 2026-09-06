# The Notes

An Unreal Editor plugin for leaving developer notes in the level itself. A note is a marker standing where
the problem is, carrying a title and a body; you read it by putting the cursor on it in the viewport, and
you never enter Play Mode to do it.

It is a way to talk about a place in the world while looking at that place. It ships no content, and
nothing it does is written into your maps.

## What it does

**A note stands in the scene.** Right-click anywhere in the level viewport and pick **Create DEV Note
Here**, or press **Shift+N** to put one wherever the cursor is pointing. The note appears already
selected, so the title goes straight into the details panel — and if exactly one actor was selected when
you made it, that actor's name is already in the title, because a coordinate says where a problem is and
not what it is attached to.

**Hover to read it.** Put the cursor on a note and its panel opens next to it: the title in orange, the
text in white, the author signed along the bottom. The panel is sized to what it holds rather than to a
fixed rectangle, so a one-line note does not cover the viewport. It hangs off the note, not off the cursor
— it labels the thing, not the screen.

**Shift+P pins it open**, so the note can be read while the thing it is about is being fixed. **Show All
DEV Notes** puts every note's title on screen at once — titles only, because a dozen full panels is a wall
of text with a level somewhere behind it.

**Every note in one list.** The **DEV Notes** tab (Window → Tools) lists notes from every level, not only
the open one: author, collection, title, level, and when the note was written and last changed. Every
column sorts. The filter box takes words to look for anywhere, or `author:`, `level:` and `collection:` to
name a field — every term has to match. A **This level** box narrows the list to the map that is open.
Double-click a row and the viewport flies to that note; **Delete** removes the selected one.

**Notes are closed by deleting them.** There is no "resolved" flag on purpose: the store is plain text, so
a repository that commits it already remembers every note that ever stood in a level, and what it said.
Deleting is how a thought stops taking up space in a scene without stopping being recoverable.

**Notes never touch the map.** The actors are spawned transient, so a note can never appear in a level
designer's diff. The note itself lives as plain text outside the content tree: text a human reads in a
review, not an asset that has to be opened to be understood.

**One file per author, per level.** Two people writing notes in the same level write different files, so
their work merges with no conflict, and a file's path says which author and which level without anything
having to parse it. Opening a level reads only the files that level needs.

**Identity survives editing.** A note keeps its id when it is moved, retitled or rewritten, and the
"changed at" stamp moves only when a human actually changed something — dragging one marker does not
restamp every note in the file.

**Notes arriving from source control appear on their own.** The notes directory is watched, so a pull
that brings in a colleague's notes puts them in the level and in the list without reopening the map.
Where the platform cannot watch a directory — a network share, most often — the **Reload** button on
the DEV Notes tab and the `TheNotes.Reload` console command do the same thing by hand.

## Installing

1. Copy `TheNotes` into your project's `Plugins/` folder.
2. Restart the editor and enable **The Notes**.
3. Set your name in Project Settings → Plugins → **The Notes (this developer)**. Empty means the account
   the editor runs under, which is usually right and occasionally is not.
4. Decide whether note files are committed. They are text and they diff cleanly, so committing them is the
   point of the one-file-per-author layout — but nothing forces it.

Requires Unreal Engine 5.8.

## Where notes are kept

```text
<YourProject>/DevNotes/<Author>/<Level>.json
```

`/Game/Maps/L_Main` becomes `Game.Maps.L_Main.json`, so the path is readable and the file name cannot
collide with another level's. A file that will not parse is reported and skipped — never silently treated
as a level with no notes.

## Settings

Two objects, split by who owns the answer:

- **The Notes** (Project Settings → Plugins) — what a team shares: the notes directory, the sprite a note
  shows in the world and the colour it is drawn in, and the actor class a note is spawned from. Saved to
  `DefaultGame.ini`, which is the file you commit. All of them ship at their defaults, so the plugin works
  before anyone configures it — and the icon's default colour is the amber the mark already is, so setting
  nothing changes nothing. The tint recolours the built-in mark; a project naming its own **Note Sprite**
  colours that texture when it makes it.
- **The Notes (this developer)** (same place) — the name new notes are signed with. Saved per user, because
  one shared author name would sign everybody's notes with whoever set the project up first.
- **The Notes (View)** (same place) — how the hover panel looks: spacing, the width the text wraps at, the
  five colours, the three font sizes, and whether the title and signature are drawn in capitals. Saved per
  user as well: the contrast that reads over a bright blockout is not the one that reads over a night
  scene, and a preference like that must never dirty a file the team shares.

Pointing **Note Actor Class** at your own subclass is how a project with its own interaction or selection
system gets its components onto a note; the plugin never names another module's types.

## What it does not do

- **Notes do not appear in a packaged game.** A note carries a "show in game" flag and the store keeps it,
  but nothing reads it at runtime yet — the flag is reserved, not wired. Files outside `Content` are not
  staged into a build by any packaging setting either, so this is two problems and not one.
- **No history.** A note is its current text. What it said last week is whatever your version control says
  it said.

## What it does not depend on

Engine modules only — no third-party libraries, no other plugins, no content packs. The note sprite is read
from the plugin's own `Resources` folder rather than shipped as an asset, so a project gets a note that
looks like a note without importing anything. Uninstalling leaves your project as it was, minus a folder of
text files you can read without the editor.

## Licence

© 2026 Kentron Cowboys. All rights reserved.
