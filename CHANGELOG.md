# Changelog


## FullDex 1.6.1 (not released yet)

- **A Spanish (or other full-size) DS game is its own game again, not a ROM hack.** 1.6 treated a full-size dump that matched no listed release as a hack: the Hub showed it read-only and it played from a save of its own. Only a trimmed ROM is a hack now. A save played that way since 1.6 is put back as the game's own, and the save it replaces is kept as `.before-1.6.1`.
- **Auto Save State and Resume on Start (DS and GBA games, both off by default)** (#214). Settings > Playback in DS games, Controls in GBA games. FullDex keeps a separate state every minute, when the app goes to the background and when you quit, and can start the game from it next time. Your in-game save and your manual slots are left as they are, and a changed ROM or hack gets its own automatic state.
- **A save state cut short no longer stops its slot working.** If FullDex closed while writing a state (a crash, the power going), that slot refused to save again until a file was deleted by hand. The half-written file is now kept aside as `.pending.failed` and the slot saves as normal.
- **Reset Game shortcut** (#168): Controls > Keys and Buttons > Reset Game. It has no key or button until you choose one.
- **RetroAchievements: a card when you unlock an achievement**, with its badge, title, points and how rare it is, and a Game Mastered card when you finish the set (thanks OzWookiee). It uses FullDex's own glass style; RetroAchievements > Unlock Banner > Classic gives the pixel-font card instead.
- **Settings answer to the controls you can see** (thanks to the input work in #210). On Off/On pills, arrows and picture cards, a tap or click changes the setting directly; the mouse wheel scrolls settings pages and lists; tapping outside a pop-up cancels it; rows that open a page say so and show their current setting underneath. A and Enter still change a setting, as before.
- **UI Size** (thanks a1ex4): larger menus and overlays for small screens such as 640x480 handhelds. Auto picks 1.5x below 720 lines; 1x to 2x can be chosen.
- **GBA pause menu: swiping scrolls it** (#203) instead of switching a cheat on or off under your finger.
- **Right stick zoom works when Swap Screens is on another button** (#206).
- **Linux:** the ROMs folder is chosen with your desktop's own folder picker (KDE or GNOME), and Cancel really cancels (#200). On screens of 120 Hz and up, a graphics driver that ignores vsync (as with NVIDIA PRIME offload, `DRI_PRIME=1`) no longer makes the game run about 3.5 times too fast (#211).
- **HeartGold and SoulSilver battles (and the hacks built on them, as Heartfelt Gold):** the Mega Evolution move list now also works in the white-background battles (in buildings and Gyms, as Jasmine's) and while the button flashes from a press.
- **HeartGold and SoulSilver (and the hacks built on them, as Heartfelt Gold): a battle's Yes / No buttons on their own.** "Make it forget another move?", "Will you switch your Pokémon?" (the Switch battle style, before the other trainer's next Pokémon comes out) and the battle's other two-button questions show only their two buttons, made smaller, one above the other at the right end of the band below the battle, with the message box beside them, as the battle's commands are laid out (and as Black and White lay out these questions): nothing of the battle is covered (they had covered the player's HP box). They are cut along their own rounded outlines, without the band of the touch screen's background round each (white in a building, light green in grass) or a frame, and the message box reaches them on a wide view and is drawn a little smaller on a narrow one when its question is long. "Will you switch your Pokémon?" had not been recognised (its "Switch Pokémon" label crossed the spot where the buttons were looked for), so the whole touch screen was pasted over the battle instead. The game's cursor and a pressed button's flash show as on the DS, and the buttons go as soon as one is pressed.
- **3DS games:** a decrypted dump without the NoCrypto marker is recognised instead of being called encrypted (#209, thanks OzWookiee). An install cut short is repaired on the next launch, including deep Windows folders with long paths (#208). The game's own renderer details now go into game.log, to help with slow-speed reports (#218).
- **Android:** two crashes at start-up fixed: one while the screen was still being set up, one when the graphics chip refused to start (the processor now draws the game instead). The README explains where Android saves go and how to choose your own Saves Folder (#205).
- **Game Options in the launcher** keeps its focus and opens the selected game's menu under the button (#191).
- **About > Credits:** OzWookiee, Saiborg23 and elripalda.
- **Report A Bug: the details, ready to paste** (Settings in the pause menu, and About in the launcher). It copies the version, platform, game, display settings and the last 200 lines of `game.log` to the clipboard, and writes `diagnostics.txt` beside the log for attaching. Nothing is sent anywhere: paste it into Discord or a GitHub issue.

## FullDex 1.6 (16 November 2026)

- **Linux and Steam Deck at 120 Hz (docked) stay at 60 fps** (#173): the graphics driver now holds each picture for two refreshes; FullDex timed it itself from when the last picture was handed back, which on the Deck docked at 120 Hz came back late and cost a refresh, so the games ran at 40. The GBA games too, and the same at 180 and 240 Hz (three and four refreshes a picture).
- **ROM hacks that are trimmed (Renegade Platinum) are known as hacks again.** A smaller ROM was read whole, matched none of its game's releases and was then left "unknown", so it was listed, played and synced as its game, with its game's save (Platinum's for Renegade Platinum). It is now a hack with its own name and save, and Manage Saves checks a ROM first. A hack's save is also looked for in the saves folder (since the tidy folders), not beside the ROM.
- **The version on the About page:** FullDex 1.6, also in the log (the releases are numbered from now on).
- **FullDex Nav in the GBA games** (Ruby, Sapphire, Emerald, FireRed, LeafGreen and the hacks the Companion knows): the DS games' watch, opened with a click of the left stick (N on a keyboard) while you play, the game running behind it. Fly opens the Fly page (the towns, and Free Flight), Party, Open PC (your boxes), Map, Pokédex and Bag open the Companion's pages, Repel uses the strongest Repel in your Bag at once, Encounters counts wild battles (shown on the picture if you like, SELECT sets it to 0) and Reset resets the game (A twice). Before, the left stick opened the pause menu in the GBA games.
- **FullDex Nav on phones and with the touch controls** (DS and GBA games): the watch is smaller, its text bigger, and it sits on the right clear of A and R instead of in the middle (the Wheel style stays in the middle). The "A: Use B: Close" hint is left out there, as the apps are tapped. The GBA games get the same four Nav styles as the DS games (Display, Nav Style: Modern, Classic, Simple, Wheel).
- **Back from a page FullDex Nav opened goes back to the watch** (DS and GBA games): Party, Map, Pokédex and Bag (and Open PC and Fly in the GBA games). Before, Back went to the Companion's menu and then the pause menu. A page opened from it (a Pokémon in Party) still goes back to its page first.
- **Pokémon moved or swapped with the Hub are in the Pokédex** (#185): the game they arrive in now has them as seen and caught, as a trade in the game does (an evolution on the way too). Before, they were in the box or party but not in the Pokédex.
- **Cloud saves and the Hub with the tidy FOLDERS** (#186, #187, #188, #189, #190; PR #192 by OzWookiee): the Hub reads and syncs the save the game plays (it read the settings folder's, so Black and White's boxes, party and Pokédex looked out of date); the sync when a game starts and after a save finds the Hub's cloud settings again; a save that cannot be read is never replaced from the cloud; a failed sync says why, and an expired Google sign-in offers Sign In again; a crash when quitting during a sync.
- **Steam shortcuts** (PR #197 by OzWookiee): ADD TO STEAM in a game's menu (A on a game) and STEAM LAUNCHER in Settings, Shortcuts, with the box art; Steam must be closed; your other Steam shortcuts are kept (the file is backed up first). Without Steam the same item adds a desktop shortcut.
- **Widescreen for Generation 3 Evolved v1.0.22** (PR #184 by Saiborg23), as Radical Red and Unbound have it.
- **RecompLauncher** (PR #182 by elripalda): FullDex can be installed and updated from RecompLauncher.
- **NAV STYLE: OFF** (#181; DS and GBA games, Display): the left stick works as it did before FullDex Nav, the next screen layout or companion tab in the DS games and the pause menu in the GBA games. The watch still opens with its key (Tab in the DS games, N in the GBA games) or the NAV button on a phone.
- **A game started from a Steam or desktop shortcut ends with it** (#196; Windows, Linux, Steam Deck, Mac): QUIT goes back to Steam or the desktop, not to the game list. Starting FullDex itself still goes back to the list.
- **Repel from FullDex Nav in Black 2 and White 2**: the strongest Repel in your Bag is used (one taken from the Bag) and the Companion shows its steps, as in the other DS games. Before, Nav's Repel said "not in this game yet" there. The game counts the steps down as you walk and asks to use another when it runs out.
- **A 3DS dump opened with FullDex** (Open With, a double-click, a Steam shortcut with the file, Linux / Steam Deck and Windows) now opens the 3DS game: the list, with its code being made the first time, then Play. Before, the file was handed to the DS player, which showed green bars in a window called "Pokemon SoulSilver". That window is now called FullDex when no game is known. Fixed too: quitting FullDex after a 3DS game's code had been made (or while it was being made) ended with a crash on Linux.
- **A 3DS file with your ROMs that is not listed says why** (#155): an encrypted Omega Ruby or Alpha Sapphire dump (decrypt it first, with GodMode9 on your 3DS), a .cia (FullDex plays .3ds dumps of the cartridge), a .cci (rename it to .3ds), another 3DS game, or the download's 3ds folder missing beside FullDex. Before, an encrypted dump was listed and then could not be prepared, and the others were left out without a word.
- **Omega Ruby and Alpha Sapphire saves in Manage Saves and cloud saves** (Windows, Linux, Android, Switch): their `omegaruby.sav` and `alphasapphire.sav` (the copy of the game's save their program already keeps with the other games' saves) now have Manage Saves (Healthy, Restore A Backup, Export, Import) and sync with cloud saves, and follow a saves folder of your own. The launcher keeps the copy and the game's own save the same too, before a game starts and when it ends: whichever was written last wins, the one it replaces backed up first, and a backup is made each time the game starts. On the Switch the copy is in the game's folder's `saves` folder, where its program keeps it.
- **Zoom and look around in the DS games** (beta): on the overworld a click of the right stick (C on a keyboard) zooms out to 1.5x and 2x and back, the right stick (I, J, K, L) looks around, and walking brings the view back to you. Text, menus and battles come back close by themselves (AUTO ZOOM IN, under Display). In Black, White, Black 2 and White 2 more of the map is kept loaded above and below while it is on, so a zoomed-out view finds land. Everything else stays: high-resolution 3D, 60 fps, the one-screen layouts.
- **Zoom and look around in the GBA games** (Ruby, Sapphire, Emerald, FireRed, LeafGreen, Ultra Violet and Monomyth): the same controls, out to 1.5x and 2.2x, the world past the screen rebuilt from the game's own map (adapted from PokéDaisy). The HD filter, LCD effect and colour correction stay on while zoomed.
- **Fly from FullDex's menu in the DS games**: Fly on the Location page, Quick Actions and Extras lists the towns you can fly to and takes you there (HeartGold, SoulSilver, Platinum, Black, White, Black 2, White 2; adapted from PokéDaisy). It needs a Pokémon that knows Fly, as in the game.
- **A fuller Pokédex, read from your own game.** In the DS games (Companion, Pokédex, then A on a Pokémon) each entry now has its battle picture and shiny picture, where to find it (place, how and levels), catch rate, gender ratio, egg groups, hatch steps, height, weight, EV yield, what it takes 4x, 2x, half, a quarter or no damage from, its TMs and HMs, move tutor moves and egg moves, and the list can be shown by type. The GBA games get a Pokédex tile in the pause menu with the same pages (Ruby, Sapphire, Emerald, FireRed and LeafGreen in every language; stats and abilities in Radical Red and Unbound). ROM hacks show their own changes (Renegade Platinum's moves and tutors, Radical Red's abilities). Nothing from the games is bundled.
- **The pause menu is a grid of ten tiles** in the DS and GBA games: Resume, Save, Load, Fast Forward, Cheats, Pokédex, Companion, Achievements, Settings and Quit, in two rows of five. The Pokédex has a tile of its own (it was inside Companion), and Fast Forward is a tile again, with whether it is on in its corner.
- **Companion for the GBA games** (Ruby, Sapphire, Emerald, FireRed and LeafGreen, every language), as the DS games have it: Party (each Pokémon's stats, IVs, EVs, nature, ability and moves), Bag (every pocket), Location (where you are, the wild Pokémon there with how you find them and their levels, and in Ruby, Sapphire and Emerald the region's map with you on it), Battle Info (in a battle, the foe's types, ability, HP and stats, how hard each of your Pokémon's moves hits it, and what it takes extra or less damage from) and Guide (answers to common questions, each hidden until you choose it). Read from the game as it runs; nothing from the games is bundled.
- **Gameplay options for the GBA games** (Settings, Gameplay; adapted from PokéDaisy): Instant Text, Reusable TMs, Run Indoors, Wild Encounters (Normal, 75%, 50%, 25% or Off), Shiny Odds (up to 1 in 128; Pokémon already caught never change), and separate Battle, Text, Bike and Surf speeds without fast forward. Each is found in your own ROM by the code it changes, so every language works, and hacks that kept that code; a game only shows the ones it has. The game file is never changed.
- **Day and Night for the GBA games** (Display): outdoors follows your clock, a warm dawn, plain day, an orange dusk and a blue night. Only the colours as they are drawn change; text boxes, menus and battles keep their own.
- **Following Pokémon for the GBA games** (Display): your first Pokémon walks behind you, in the games Zoom and Look Around supports. Its walking sprites come from your own HeartGold or SoulSilver in the ROMs folder, read from the ROM each time and never saved; the very biggest Pokémon (Steelix, Wailord, the box legendaries and a few more) have no sprite there, so they don't follow. It appears after your first step and stays off while surfing or in a battle.
- **Use, give and take items from the GBA Companion** (Companion, BAG and PARTY): a Repel keeps wild Pokémon away; medicine, vitamins and PP items go to the Pokémon you choose, each greyed where the game would say "It won't have any effect"; any holdable item can be given (what it held goes back in the bag); in PARTY, a Pokémon's held item can be taken. Done by the game's own item code, so nothing is ever lost. Ruby, Sapphire, Emerald, FireRed and LeafGreen.
- **Teach TMs and HMs from the GBA Companion's BAG**: TEACH opens the game's own "Teach which POKéMON?" screen, with ABLE / NOT ABLE / LEARNED, forgetting a move and the game's rules for the TM, and goes back to the field when it closes. Emerald, FireRed and LeafGreen (Ruby and Sapphire's screen can only go back to their own Bag).
- **The foe's team in the GBA Companion's BATTLE INFO**: in a trainer battle, the trainer's whole team in order and how many are left. The Pokémon out and any already seen or hurt show by name with their level and HP (each with its own card beside it); the rest stay hidden as "? ? ?" until they come out. Ruby, Sapphire, Emerald, FireRed and LeafGreen.
- **Boxes in the GBA Companion** (Companion, BOXES): every PC box as it is now, each Pokémon with its full card (level and stats worked out as the game does when it leaves the box). Choose one to take it into your party or swap it with a party Pokémon; in PARTY, choose one to put it in the current box. Done the way the game's own PC does it, with its rules (not your last Pokémon that can fight, Mail removed first), on the field. Ruby, Sapphire, Emerald, FireRed and LeafGreen.
- **Fly from the Companion in the GBA games** (Companion, FLY): pick a town you've been to and the game plays its own Fly, cut-in and landing included. It asks what the move asks: a Pokémon in your party that knows Fly, the badge for it, and a place you could fly from. Ruby, Sapphire, Emerald, FireRed and LeafGreen (the English releases; each game's code is checked first, so a version it doesn't know just has no FLY).
- **Free Flight in the GBA games** (Companion, FLY, FREE FLIGHT): ride the game's own Fly bird over the map yourself, through trees, water and mountains and across into the next route or town. The D-pad steers, A held goes faster and B lands on the nearest place you can stand. Ported from PokéDaisy's Free Flight (its shadow, clouds and wind lines too). It asks what Fly asks: a Pokémon that knows Fly, its badge and a place you can fly from.
- **Fly On** (Settings, Gameplay): what Free Flight carries you on. The game's bird, your own Pokémon that knows Fly (its sprite from your HeartGold or SoulSilver, the player riding on its back), any Pokémon you like (picked from a list in Companion, FLY, FLY ON, with a preview of its sprite: every one up to Arceus that HeartGold has a sprite for, and without HeartGold every one up to Deoxys as the game's own party icon), Emerald's own Latias, Latios, Rayquaza, Lugia, Ho-Oh and Deoxys, or in FireRed and LeafGreen their own Articuno, Zapdos, Moltres, Lugia, Ho-Oh and Deoxys (from your HeartGold or SoulSilver where it has them; without those games, Emerald's own story sprites, or the game's own party icon, so they still face the way they fly), or PokéDaisy's air balloon, helicopter, zeppelin and plane.
- **Clouds over unexplored places** (Settings, Gameplay, on by default): while you fly, a bank of clouds covers every map you haven't stood on yet and clears as you reach it. FullDex notes the maps you walk through from now on, kept beside your save; for a save from before, the towns you have visited and the routes next to them count as seen.
- **GBA fast forward music plays each song in full** at its own speed and pitch (adapted from PokéDaisy): a hidden copy of the game records each song the first time you hear it, cut where the song loops, and fast forward plays that recording from where the song has got to. Recordings are kept between sessions (up to about 400 MB). Songs that end by themselves keep the old fast forward sound.
- **Battle buttons in other languages** (contributed by a1ex): a label wider than the English one (ATTAQUE, FUITE) is no longer split across the button.
- **Companion no longer costs frame rate.** Its frosted glass, which read the screen back and blurred it while the Companion's window or HUD was up, is off by default: the panels are dark glass instead. FROSTED GLASS on the Companion page turns it back on.
- **BATTLE BUTTONS: SUN AND MOON** for the DS games: FIGHT across the top in red, POKÉMON, BAG and RUN below it, in the later games' colours, with the D-pad and A working as well as touch.
- **Credits**: PokéDaisy is credited on the About page and at the bottom of the README; the in-game "Based on PokéDaisy" lines are gone.
- **Touch-only screens with a controller: Diamond, Pearl, Platinum, Black and White.** The right-stick touch cursor now comes on by itself for Poffin cooking, Underground digging, contest dressing up, Ball Capsule seals, the Musical's dressing up and the Trainer Card signature, as it already did in HeartGold and SoulSilver. It never did before: those screens were looked for but not watched.
- **Platinum, Diamond and Pearl: Poffin cooking with the pot large.** In the one-screen view the pot fills the height of the picture, the top screen beside it smaller (it was half size, side by side), and HYBRID shows the pot as the large screen (it was the small one).
- **Black and White: the Musical's dressing up with the touch screen large** in the one-screen view (it was half size beside the curtain), so the props are easier to drag.
- **Diamond, Pearl and Platinum: the contests' dressing up on one screen.** The touch screen large, with the theme along its top and the countdown heart large beside it, cut from the top screen (whose message box is empty while the clock runs).
- **Platinum, Diamond and Pearl: the Underground in the one-screen view.** The games put the world on the touch screen there and the radar's map on the top one; the one-screen view showed the map with the world drawn around it, so you could not see yourself. Now the world is shown widened across the view, as the overworld is, with the radar's map over its top left corner.
- **Credits**: CharizardMT (GreninjaMaster89) for the Black and White battle buttons, the HG-Engine Mega Evolution button and the HeartGold / SoulSilver layout fixes, meltingmagnetz for help with the Pokédex, and Jeezer for the tidy folders idea, on the About page.
- **FOLDERS OF YOUR OWN**: SAVE STATES FOLDER, CHEATS FOLDER and SETTINGS FOLDER (Settings, Game List), and SAVES FOLDER on Android too: point each at a folder of your own (a Syncthing or RetroArch one, say); what you have is copied there, nothing replaced. The folder picker on Windows, Android, Linux (zenity or kdialog) and macOS.
- **FOLDERS**: saves, save states, cheats and FullDex's own settings files each have a folder of their own (saves, states, cheats, settings), so the ROM folder keeps only the ROMs (on a phone, the rest in one FullDex folder inside it). New installs start that way; an update moves your files once, carefully (each copied and checked before the original is removed), and says so. A saves folder of your own is left as it is.
- **FULLDEX NAV: REPEL** (DS games: HeartGold, SoulSilver, Platinum, Black, White): uses one from your Bag, the strongest you have, as using it from the Bag would. Lit only when you have one.
- **iPhone and iPad: the game list turns with the device.** It stayed upright whichever way you held it; now it follows the device, landscape too (upside down as well on an iPad). Games are still sideways.
- **Linux and Steam Deck: Omega Ruby and Alpha Sapphire (3DS, beta)**, as on Windows: put your own dump (.3ds) with your ROMs, choose it once to prepare it (5 to 15 minutes), then play. In the x86_64 download (AppImage and tar.gz); on ARM Linux they stay as Coming Soon.
- **FULLDEX NAV, a quick menu on a watch** (DS games): L3 (the left stick pressed), N on the keyboard or NAV on the touch controls (beside L) opens it while you play. Fly, Party, Open PC, Encounters (how many wild Pokémon you have met, on its icon; kept beside your save, it can also show on the picture), Reset (A twice), Map, Pokédex and Bag, each a tap or a press away. NAV STYLE (Display) chooses MODERN, a watch with a graphite case and a glass face, CLASSIC, a red watch, SIMPLE, a plain glass card, or WHEEL, its apps alone in a ring that the stick points at. The left stick's press no longer cycles the companion's tabs.
- **ASPECT RATIO in the launcher's Display settings,** as well as in the game's own Display menu (one choice for both).
- **The companion window keeps clear of your Pokémon's HP in a battle.** While a battle is on, it ends half way down the screen, above your HP box, in every game; the rest of the time it is as tall as before.
- **BATTLE INFO says when a game's battles can't be read yet** (HeartGold, SoulSilver and the carts whose battles are not mapped), instead of "No Battle" in the middle of one.
- **Android: a second display keeps up with the game.** On dual-screen handhelds and phones with a second display (SECOND SCREEN under Display), the DS screen there is drawn straight to that display at the game's own frame rate (it fell to about 50 a second, unevenly), with sharp pixels instead of a blurred picture, and COMPANION there no longer makes the game stutter: it is drawn again only when what it shows changes, on dark glass unless FROSTED GLASS is on.
- **Android: a game on your home screen.** A game's menu has Add To Phone Home Screen: a shortcut with the game's picture that starts that game straight away (Android 8 and later; the phone asks where to put it).
- **Online play: the DS games can connect again** (#149). The console's Wi-Fi now reaches the internet (through melonDS's user-mode network), so a custom DNS set in the game's Nintendo Wi-Fi Connection settings (Wiimmfi, Pokemon Classic Network) works on Windows, Linux, Steam Deck, Mac, Android, iPhone and iPad. Not on the Switch yet.
- **HeartGold and SoulSilver: the Trainer Card shows its badges** beside the card in widescreen, as Platinum's does. Only the card was shown, so the badges needed a two-screen layout.
- **Fast forward stays on while you walk** (#156) in Black, White, Black 2, White 2 and the games' other languages: every direction pressed dropped it to normal speed for a quarter second. In the GBA games, fast forward switched on (Toggle, or the menu) also stays on whatever is pressed.
- **The game list shows Pokemon games only** (#152): other GBA ROMs in the folder (Wario Land 4) are left out.
- **Linux AppImage and Steam Deck: ROMs beside the AppImage are found** (#150), loose or in a roms folder next to it, as well as in ~/.local/share/pokemon-ds-wide/roms.
- **ROM hacks with another region's game code play** (#134, HeartGold Origins): a changed Japanese, Korean, German or Italian ROM was refused as "not validated"; it now plays as a hack with its own save.
- **4:3 screens: the launcher's pages fit** (#144, Retroid Pocket Nova): Launcher, Hub, ROMs and About kept their list column so narrow that the names overlapped. The ROMs page's details and the About page's QR codes now leave the buttons room.
- **iPhone: the game list keeps clear of the Dynamic Island and the home bar** (#153).
- **Widescreen for Emerald, FireRed and LeafGreen in French, German, Spanish and Italian, and Japanese Emerald** (#161). Before, only the English releases had it. (Japanese FireRed and LeafGreen are laid out too differently for now.)
- **A combined Pokédex in the Hub** (#165): every Pokémon caught or seen in any of your games' saves, in one Pokédex. The National Pokédex and each region (Kanto to Alola) with how many are caught and seen, and each Pokémon's icon: in colour if caught, dimmed if seen.
- **GBA, widescreen: the START menu sits at the right edge of the wide picture** (#131), in Emerald, FireRed and LeafGreen, and in FireRed and LeafGreen its help bar along the bottom reaches both edges. Before, they stayed at the GBA's own width in the middle.
- **Widescreen for popular GBA ROM hacks: Radical Red 4.1, Unbound 2.1.1.1, Emerald Legacy 1.1.4, Inclement Emerald 1.13 and Monomyth 1.1.** Each has a widescreen patch of its own, made for that release (Radical Red and Unbound from FireRed's, as they keep its code where it was; Emerald Legacy and Inclement Emerald built from their own published source; Monomyth's matched to its own build of Emerald's code, with its map layers kept clear of the speaker portraits). Put the hack's ROM, or its patch beside your ROM, in the ROMs folder as usual. The DS hacks tested (Renegade Platinum, Following Renegade Platinum, Sacred Gold, Heartfelt Gold, Volt White 2 Redux, Following White 2) already play in widescreen and at 60 fps like their games.
- **GBA ROM hacks that can't take their game's widescreen patch now play as they are** (#163): ones rebuilt from the decompilation, whose code is arranged differently, and ones using the space the patch is written into. Before, the patch was put on anyway and wrote over the hack: Emerald Legacy froze after the professor's introduction. Hacks that only changed parts the patch replaces still get widescreen, as before.
- **iPhone and iPad: a controller's B no longer opens Fulldex's menu, and one press on the D-pad moves a menu's cursor once** (#166). iOS also passed the controller's buttons on as keyboard keys (B as Escape, the D-pad as the arrow keys), so each press counted twice. Those keys are now ignored unless a keyboard is connected.
- **No more shimmering pixels on screens the DS picture does not divide into evenly** (#69), such as a 720-line phone or the Steam Deck. With SMOOTH SCALING off, the DS pixels used to come out in two sizes (3 and 4 lines on a 720-line screen), which shifted about as the camera moved. Now every pixel is the same size and only the seams between them are blended. With SMOOTH SCALING on, the game picture is now softened with widescreen off too (before, it stayed sharp).
- **Diamond, Pearl, Platinum, HeartGold and SoulSilver: a trainer battle's party Poke Balls no longer stretch** in widescreen (#159). They are moved out with their bar, as the HP boxes are, instead of being stretched with the background between the two sides.

- **Pokémon Omega Ruby and Alpha Sapphire join the list, in beta** (the 3DS Beta tab; their tiles say 3DS Beta), played by their own native programs from the player's own dump. **Windows:** a dump (.3ds) in the roms folder is listed; the first time it is chosen, its code is built from the dump once (5 to 15 minutes, the list showing how it goes, other games still playable), then it plays and quitting it returns to the list. The program comes in the download's 3ds folder; each game keeps its own folder and saves there. **Switch:** the folders the FullDex Builder makes are listed (one tile a game) and started from the list. **Android** (64-bit phones, with the Omega Ruby project's kit built into the app): a dump in the ROM folders is copied into FullDex the first time it is chosen, then plays on a screen of its own and returns to the list. The first Play copies the dump in (after checking there is room) and then starts it. The game's menu has Build For Best Speed (on the phone, about 25 minutes; Game Code Built once done, and the tile says 3DS · Built) and Use Game Code From PC (the FullDex Builder's). In the 3DS games the FullDex menu works by touch, the game's touch screen takes quick taps, the on-screen Start and Select reach the game, the game uses the screen round a phone's camera, and Display has Battle Button Size (80 to 160%, 120% on phones). Their COMING SOON tiles are gone.
- **Catching a Pokémon: the nickname question no longer covers it** (#62). Only the Yes and No buttons are shown, at the right above the message, in every aspect ratio. In widescreen the Pokémon just caught is no longer cut in half.
- **HG-Engine hacks (Heartfelt Gold and others): the Mega Evolution button has its place in the battle panel.** When a Pokémon can Mega Evolve, the move list is no longer replaced by the whole touch screen pasted over the battle: the button sits at the right end of the panel beside the four moves, in every aspect ratio. Right from the moves picks it, A turns Mega Evolution on or off (its symbol lights up as in the game), and left goes back to the moves; clicking or tapping it works too.
- **HeartGold and SoulSilver: the radio's words stay in view in widescreen.**
- **HeartGold and SoulSilver (and the hacks built on them, as Heartfelt Gold): the text boxes, the Running Shoes button and FIGHT sit straight.** In widescreen the overworld's text box, in every frame style, and the sign with the area's name no longer have bars of their frame's colour sticking out of them to the edges of the view. In the single-screen view the Running Shoes button beside the X menu has its shoes in its middle (they sat to the right), and FIGHT's Pokémon icon and label are in the middle of the button, not left of it and against its top edge. The X menu's panel no longer has a strip of the touch screen's grey background under it or a thicker edge on its right, its edge is the same all round, it sits in the middle of the view's height, and its MENU heading is in the middle of the panel.
- **Black, White, Black 2 and White 2: the battle's buttons look like part of the battle, not a piece of the touch screen.** With BATTLE BUTTONS at STRIP (now the default in all four games, on any screen shape), FIGHT, BAG, RUN and POKÉMON sit on the message box as rounded buttons like the moves, in the same two rows the moves take: FIGHT across the top, BAG, RUN and POKÉMON under it, each with the game's own label and colours (and they light up when pressed, as the game's do). In double and triple battles FIGHT shows your Pokémon's icons beside its label. The moves and the targets in double and triple battles keep their own shapes, without the touch screen's Poké Ball behind them, and back, SHIFT and the rotation buttons beside them are rounded too. The game's cursor stays on the button it is on, clicks and taps land on the button they look like they land on, and the rows of Poké Balls for each side's party sit under the HP boxes. The question about switching Pokémon has its answers on the message box the same way, the HP boxes no longer jump back to their old places while it is asked, and the view no longer goes black for a moment as the party or the Bag opens. CORNER and PANEL are still there.
- **Fast forward on Android is smooth, and faster.** Frames nobody sees no longer draw their 3D picture, the 3D thread keeps to the fast cores, and pictures reach the screen at an even pace. On a Pixel 9 Pro XL at 4x it went from about 2.2x with uneven pictures to 2.4x with even ones. The speed chosen is the most it will go: a phone runs as fast as it can keep smooth.
- **DS fast forward: the music plays at its normal tempo and pitch** on Windows, Linux and Android, by playing the game's own music on a second, silent copy of the console (adapted from PokéDaisy). Switch, Mac, iPhone and iPad keep the earlier way for now.
- **GBA games: the pause menu is the DS games' glass menu**, and **Save and Load have ten slots** with a picture of where each was saved, as in the DS games. A state saved before this update is Slot 1.
- **Windows: steadier frame times** (#17). The wait before each picture woke up late on the system timer, about one picture in five on a 240 Hz screen; it now uses a precise timer.
- **Android: WORLD ON GPU.** The widescreen world is drawn by the phone's graphics chip (OpenGL ES 3.2), with the processor as a fallback. On a Pixel 9 Pro XL this cut the power used at 60 FPS by about two thirds. The frame clock also follows the screen's own 60 Hz.
- **Touch controls: hide single buttons and choose how much they show.** In Touch Controls, HIDE takes the picked control off the screen (it stays in the editor, dim, so SHOW brings it back) and VISIBILITY sets them all to 100, 75, 50 or 25%. DS and GBA games; MENU always stays. Each control could already be resized on its own.
- **Android: the in-game menus scroll by touch** (#117), also when the swipe starts on a heading.
- **Moving Pokémon between games: trade evolutions keep the right Ability.** A Gen 4 Pokémon also keeps its personality value. (One already evolved this way can't be repaired, but the hub's backup of the save can be restored.)
- **Launcher: Continue starts the last game** when tapped or clicked (it opened the game's menu), and **Android's Play starts a game found in the app's own folder** instead of asking for a ROMs folder first.
- **GBA ROM hacks try widescreen** as their base game does.
- **Black 2 and White 2: tidier edges** on the title screen and the banner corners in widescreen.
- **New text translated** into Spanish, Italian, French, German, Japanese and Korean.

- **The Tiles carousel slides.** The covers move and grow smoothly to the next game with the arrows, a click or a swipe, and follow the finger or the mouse while it drags.
- **A ROM hack is shown by its own name** (Renegade Platinum, not Platinum) on the home screen.
- **The game list finds the controller again after a GBA game** on a computer.

- Choose Hold or Toggle for the fast-forward button in launcher settings and in-game playback settings, shared by DS and GBA. Toggle works with a keyboard or controller, and with the DS on-screen button; the GBA on-screen button stays a tap on, a tap off.

- **Android restores fullscreen after resume/unlock.** Reapply system-bar and layout state when focus returns, with a bounded retry after the unlock transition. Windowed/multi-window mode, open keyboards and the chosen cutout preference are preserved. Retroid Pocket 6 hardware confirmation is pending.

- **Layout choices have mini previews.** Sidebar, Grid Only and Tiles appear together in the preview panel, with the active choice outlined. Tap a miniature to select it; phones show their two supported grid layouts.

- **Grid Only launcher layout** hides Continue Playing and uses its space for more same-sized game cards. Sidebar and Tiles remain available.
- **Launcher swipes coast gently and the mouse wheel scrolls the grid smoothly**, including fractional trackpad movement. A new press, controller navigation or an open menu stops coasting.
- **Click the left stick to cycle visible companion tabs.** Existing custom button bindings take priority. Battle automatically opens its tab once when combat begins, allowing manual tab changes during the battle.

- **The launcher grid shows partially visible rows.** Games fill the available grid area and scroll through its edges instead of disappearing until their whole card fits. Headers and bottom controls stay clear.

- **Fixed aspect ratios keep world geometry aligned on ultrawide screens** (#139). The 3D world, 2D overlays and coverage checks use the same selected view width instead of stretching the world beneath the side bars.

- **Fast-forward sound stays close to the picture** (#123), in the DS and the GBA games. The music keeps its own speed and pitch, and what is heard is never more than about two seconds behind the game (it could fall half a minute behind). Letting go of fast forward goes straight back to the live sound.

- **ROM imports preserve different games sharing a filename.** An incoming ROM cannot overwrite a different existing ROM. Failed GBA imports also cannot launch the old same-named file, and read failures no longer count as identical file contents.

- **Wi-Fi firmware settings survive failed replacements.** The previous settings file is retained when a write cannot be installed; Switch keeps a rollback copy, and puts it back or clears it at the next start or save if a save was cut short. Oversized or incomplete firmware files are rejected with bounded memory use before starting a DS game.

- **Android document-provider folders can import DS and GBA ROMs** (#124). Declining broad file access no longer cancels the folder picker. Unsupported filesystem providers use bounded background imports with clear success/failure messages; source saves are never imported or overwritten. Existing save locations are pinned before changing ROM folders.

- **Android frontends can open a DS ROM directly** (#98). Granted document links are imported safely when no ordinary file path is available; failed imports keep the previous file. An active game is never replaced by a new request.
- **Direct DS launches keep save checks and the existing save folder**, including files outside the selected ROM library.

- **Optional battle timing logs** record game-frame rate, active speed and generated interpolation frames to help distinguish speed changes from uneven animation.

- **Steam Deck game shortcuts target the selected ROM**, including hacks and multiple games sharing a code. Names and icons are kept separate, and filenames with spaces or special characters are escaped.
- **New Steam Deck setups use Start for pause**, for DS and GBA. Existing saved button choices are kept. Confirmation menus also recover safely from an invalid selection.

- **Phone confirmation dialogs are easier to read and tap.** Quit and other confirmation cards use larger text and buttons sized for touch in both orientations.

- **Very wide GBA screens keep full-screen backgrounds while hiding repeated windows** (#131).
  The level-up window no longer repeats at the opposite edge, and a background that reaches
  an edge can still fill that side of the wide view. The START-menu/help-bar width is unchanged.

- **FAST FORWARD stays as you left it** (#136). The DS and GBA pause-menu choice is remembered
  across launches and shared when changing between DS and GBA games. Held shortcuts and the
  GBA on-screen fast-forward button remain temporary.

- **Choose LCD effects before launching a game.** Launcher Display settings now include DS Screen Filter (Off, Scanlines, LCD Grid) and GBA LCD Effect, with choices shared with the in-game menus.

- **Custom-sized screenshots keep clean overlays.** Window-sized text and menus are omitted when their dimensions differ from the requested export, avoiding scrambled rows.

- **Launcher pages refresh when scrolled.** Cached page images now follow detail, list and release-note scroll positions.

- **Portrait About pages scroll inside their panel.** Support codes and long credits stay above the navigation bar, and hidden links cannot be tapped.

- **Closing the Windows launcher exits cleanly.** Its music-status worker is stopped before the application finishes, including when media controls are unavailable.

- **Launcher dialog arrows act once with overlapping inputs.** Controller remappings keep their first direction press, and navigating a dialog no longer moves the game list behind it.

- **Import game pictures on Linux and Steam Deck.** Change Picture now has a folder browser for PNG and JPEG files. Pictures for games with the same filename are kept separately.

- **Unreadable ROM and snapshot files stop cleanly when a file seek fails.** Invalid file lengths are rejected before allocating memory.

- **Modified regional DS games can use the ROM-hack launch path.** A recognised hack is no longer rejected for differing from its clean regional release; it uses normal emulation and keeps its separate save.

- **Portrait ROMs page keeps controller actions in view.** Lower buttons scroll into reach, and the entire last button can be reached by touch scrolling.

- **DS fullscreen shows the mouse or trackpad pointer while it is used.** The pointer stays visible during clicks and drags, then hides after a short idle period.

- **Launcher: overlapping keyboard and controller presses act once.** A Steam Input press arriving through both paths no longer selects twice or skips a menu level.

- **GBA menus accept mouse and trackpad clicks.** Click the pause menu and game list without enabling on-screen touch controls.

- **Choose a shared ROM folder from the launcher.** The ROMs page lets PC and Android players use an existing library without moving their saves.
- **iOS: SideStore can register the app name.** Both app names now use Pokemon Fulldex without an accented letter.
- **Pause menu on touch screens: swipe settings rows to scroll.** Swiping no longer changes the setting under your finger; tapping still selects it.
- **DS cheats can be named after a ROM.** A ROM-specific cheat file takes priority over the game-code files, and each ROM hack keeps its own enabled cheats.
- **A player's own clean game is not taken for a hack.** A copy whose program and files are a clean release's keeps the game's save and features, even where the dump differs from the listed one outside the game itself (the secure area, the padding at the end).

- **Expanded DS ROM hacks get their own saves.** A ROM larger than its clean release is recognised as modified even if its code fingerprint, or an older cached result, says clean.
- **GBA ultrawide screens: small text backgrounds no longer wrap across the picture.** The level-up window is kept from repeating at the opposite edge.
- **DS ROM hacks keep their own map memory and wild encounters.** Original-game memory modifications are no longer attached to recognised hacks.

- **Switch: always close the network before handing a game back to the loader.** The launcher
  no longer relies only on registering a shutdown callback, which the Switch runtime can refuse
  when its callback table fills up. This targets the black screen after selecting a game.

- **ROM checks use less memory before starting a DS game.** File tables and fingerprint regions
  are read in blocks. If the background check cannot start, the launcher logs the failure instead
  of closing. Switch logs now show the launch stages to help investigate black-screen reports.

- **RetroAchievements: a wrong clock is named when logging in fails.** A device whose date is
  wrong (a Switch set years ahead, often) cannot check the server's certificate, and the settings
  only said LOGIN FAILED. They now say CHECK THE CLOCK, and the message asks whether the date and
  time are right.
- **Launcher: a save named after the ROM is found.** A save brought from another emulator or a flashcart
  (`Pokemon - Edicion Negra 2.sav` beside `Pokemon - Edicion Negra 2.nds`) is used when the game's own save
  (`black2-es.sav`) is not there. Another game's save name is never borrowed.
- **SoulSilver: the wild Pokémon's slide to the right of a wide battle follows the game's frames.** It was
  timed by how many pictures were drawn, so the display rate, skipped frames or screenshots changed
  when (and whether) it happened.
- **SoulSilver and HeartGold battles: no more flash of the touch screen in the middle of a battle.** A wild
  Pokemon breaking out of a Poke Ball, an enemy's Vine Whip, or your own Pokemon being sent out after a
  switch briefly ran code of the Bag and party screens, and the view jumped to the 4:3 touch-screen
  picture (a Poke Ball background) for about ten frames. The Bag and party layout now waits for the
  Bag or party itself.
- **HeartGold and SoulSilver: Fly no longer flashes both screens at 4:3 when you land.** For about
  half a second after the flight the two screens were joined at 4:3 (the overworld above the start
  menu) before the wide view came back; the world now stays up instead.
- **SoulSilver and HeartGold: an evolution from a Rare Candy in the Bag fills the wide view.** Its
  sides carry on the scene's colours instead of black bars (only a battle had its edges continued),
  and its text box reaches across the view instead of stopping at 4:3.
- **SoulSilver/HeartGold: the Poke Mart's item panel no longer shows a sliver of the map above it.** The
  description strip taken from the top screen started a row too high.
- **GBA games: the pause menu works like the DS games'.** On the Switch, + is the GBA's START and
  + and - together open the pause menu (START BUTTON in CONTROLS swaps them); a lone + or - reaches
  the game a moment later, so the pair is never taken for one of them. The menu has the DS games'
  DISPLAY, SOUND and CONTROLS pages and CHEATS, and the settings the DS games share (ASPECT RATIO,
  smooth scaling, VOLUME in tens, FAST FORWARD SOUND, FPS COUNTER, BORDERLESS FULLSCREEN) are the
  same setting in both.
- **GBA games: fast forward like the DS games'.** The music carries on at its normal speed while
  the game runs ahead (FAST FORWARD SOUND off: silent), and a quick tap of a direction moves a menu
  cursor once (#73).
- **GBA games: new display options.** HD FILTER redraws the pixel art with smooth edges (xBRZ by
  Zenju, GPLv3), LCD EFFECT adds a faint grid like a GBA's screen, and COLOUR CORRECTION shows the
  colours as a GBA's screen did. SCALING keeps SHARP, SMOOTH and PIXEL PERFECT (whole-number zooms
  only), now explained, and ASPECT RATIO (AUTO, 16:9, 16:10, 3:2, 4:3) replaces WIDESCREEN.
- **Switch: the GBA games' sound is no longer crackly or sped up.** They asked the Switch for
  512-sample sound buffers, which ran dry between them: it played about 41,000 samples a second
  instead of 48,000, so sound piled up and was thrown away. Now 2048, as big as the DS games' and more.
- **LOAD STATE: a damaged state is refused, not half-loaded.** A state that fails partway
  (corrupt or from another version) used to report "STATE LOADED" over a half-restored
  console. It now says so, keeps your save as it was, and restarts the game on it.
  (Follow-up to #34.)
- **Settings file with Windows line endings** (edited in Notepad, or copied from Windows) is read
  properly: before, every on/off and view setting in it was ignored and the next save overwrote them.
- **Setup program: an incomplete or wrong ROM gets a clear message** instead of a Python error, and `--rom` with a missing file or value stops with a usage line.
- **Steadier frame times on screens that are not 60 Hz** (such as 144 Hz): each picture is now
  handed over at its even time, instead of after the next frame's drawing, which took a
  different time from one frame to the next. Measured on SoulSilver in the overworld at
  144 Hz: the spread of the time between pictures fell from 0.6 ms to 0.03 ms.
- **Keyboard: the key that closes the pause menu no longer reaches the game.** Choosing RESUME
  with the A key (or backing out with B, or START's Enter) passed that key on to the game for
  as long as it was held, as a controller's buttons already did not.
- **Game list: importing a save no longer destroys the one already there.** A dropped `.sav`
  with the name of an existing save now keeps the old one as `<name>.sav.before-import.bak`;
  `file://` paths with `%20` and the like import, and `--rom x.gba` is imported once, not twice.
- **Hybrid layout: a click on the small top-screen inset no longer touches the game.** With the
  touch screen large and the top screen drawn over its corner (4:3 windows), clicking the inset
  tapped the touch screen hidden under it.
- **A save that cannot be read is no longer started over.** If the save file is there but unreadable
  (wrong permissions, a failing drive), the game now stops with a message instead of starting a
  new game and writing over it at the first in-game save.
- **Pause menu: the mouse can click its rows** (PC), as a finger taps them on a touch screen;
  clicking beside the box closes it.
- **Forgetting a move (TM, level-up): the new move and the Cancel button show** on the
  widescreen summary. They were cut off below the moves list.

- **HeartGold and SoulSilver: the Apricorn Box, Pal Pad and Berry Pots screens are whole again.**
  Opened from the Bag, they were shown as a cropped start-menu panel (the Apricorn Box's message
  cut off and its CLOSE button missing, the Pal Pad's menu and the pots' row cut at the edge);
  they now show the touch screen like the other bottom-screen applications.
- **HeartGold and SoulSilver: the Vs. Recorder is whole.** Used from the Bag's Key Items, its
  "TOUCH HERE!" title and its menu (Battle Videos, Frontier Record) were shown as a cropped
  start-menu panel beside a green top screen; they now show the touch screen like the other
  bottom-screen applications.
- **Widescreen overworld: the door wipe covers the whole view.** Coming out of a building the world's 4:3 middle showed at once while only the sides were wiped open; now the black rows of the wipe cross the full width.
- **SoulSilver and HeartGold: the Running Shoes button beside the X menu has a right edge.** The game
  draws the button past the edge of its screen, so the copy was cut off flat; its outline is now
  closed on the right as on the left.
- **Cheats page: the details strip fits on small screens.** At 800x480 (Android handhelds) or
  smaller the cheat list was as tall as the screen, so a cheat's full name and the database's
  note under it were cut off the bottom. The list now leaves room for the strip.
- **HeartGold and SoulSilver: Fly's sky band fills the whole width.** The sky behind the player
  during the fly-away and landing stopped at the screen's right edge on a wide view, with the
  world showing beside it (its outermost column belongs to the world); it now goes on to both edges.
- **Graphics-chip 3D at DS resolution (`--wide-gpu`, the Switch default): shapes and textures
  sit on the same pixels as the software renderer's** (signs keep their top row, floors no
  longer shift by a pixel).

- **Pokémon summary view: the backing panel's corners are rounded in the window too.** The
  graphics-card drawing filled the panel square over its rounded outline, so the corners showed
  the panel's colour outside the curve; the headless pictures were right.
- **Smooth scaling: the Summary screen's lower half no longer loses its EXP bar** (and other
  trimmed rims): the filtered compose now honours the same rim and corner-sprite trimming as the
  sharp one and the GPU path.
- **Black 2 and White 2: battle questions keep their buttons** until you answer ("Will you
  switch your Pokémon?", "Use next Pokémon?"), in every language. The question stays full size
  with the battle, and both answers sit at the end of the message bar (Black and White too).
  (Thanks to ahrhamza, PR #35, and IamCarron, PR #70, for the other languages' addresses.)
- **Black, White, Black 2 and White 2: the Bag shows both screens side by side** once an
  item is chosen, as on the DS, so its messages are never covered and a TM's type, power,
  accuracy and PP are all visible. (Thanks to ahrhamza, PR #35.)
- **Fast forward: a quick tap moves a menu cursor once** (#73). The games repeat a held
  direction after a few frames, so at 4x a tap moved it two or three places. A direction's
  first quarter second now runs at normal speed; held longer, fast forward carries on.
- **Switch: saves in a saves folder** beside the program; existing saves (and their save
  states) move there the first time, none replaced. Only Fulldex's own folders: another app
  that keeps saves beside its ROMs is left alone. Older versions do not look there.
  (Thanks to nicolvus, PR #51.)
- **Spanish Black, Black 2 and White 2 fully supported**, like Spanish White: any copy of the
  game works, not only the exact dump the beta profiles were made from. (Thanks to IamCarron, PR #71.)

- Diamond and Pearl in more languages (experimental, like English Diamond/Pearl): Diamond in German
  and Italian, Pearl in French, Spanish, German and Korean. Each has its own save (pearl-fr.sav,
  diamond-de.sav, ...). Japanese Diamond and Pearl are not supported yet.
- Cheats for the GBA games: CHEATS in their pause menu, 15 built in for Emerald, FireRed, LeafGreen and Ultra Violet (walk through walls, max money, Rare Candies, Master Balls, shiny encounters, always catch, infinite HP and PP, double EXP and more), all off until turned on; achievements pause while any is on, as for the DS games (`frontend/gba_cheats.h`).

- Cheats built in for every DS game: 15 popular codes (walk through walls, max money, Rare Candies, Master Balls, shiny encounters, always catch, infinite HP and PP, double EXP and more) in a POPULAR CHEATS folder under CHEATS, for US, European, Spanish, German, French and Italian versions. All are off until turned on. From DeadSkullzJr's database via libretro-database (`tools/cheats/make_builtin_cheats.py`).

- Windows: allow repeated saves of a custom touch layout while preserving the previous layout if replacement fails.

- Add French, German, Japanese and Korean to the launcher and Fulldex menus, with automatic language detection, embedded fonts and translated controller/touch labels. Embedded GBA menus follow the selected language.

- Complete Italian translations for newer menu pages, translate Spanish/Italian option values inside their arrows, and fit pause menus to narrow displays. Update regional ROM guidance. Adapted from AlessioGiacobbe’s PR #9.

- Fix Platinum introduction controls so A can advance the dialogue shown before the Poke Ball touch prompt.

- Expand the regional beta to 42 supplied ROM versions across French, German, Italian, Spanish, Japanese and Korean. Japanese casino screens use the generic two-screen fallback. Korean Platinum has passed startup and menu checks; broader battle and single-screen testing continues.

- Add locally tested regional beta profiles with exact-version checks; unsupported dumps are rejected before loading a save.

- iPhone and iPad: wait for the active app scene before opening the game list, and keep rendering aligned when the available display changes.

- iPhone and iPad: adopt the modern app lifecycle to fix launch failures on iOS 27, and size the game window to the available display when folding, unfolding or resizing.

- Companion text keeps readable Latin letters when the game uses accents, rather than leaving gaps in names and descriptions.

- Battle reference pages account for selected ability/item immunities and show party move-coverage comparisons. Variable-type and special-damage moves are marked for separate checks. These are static matchups, not damage or safe-switch predictions.

- On-screen game buttons no longer cover native pause menus or intercept taps on their top rows. Game controls return when the menu closes.

- Browse regular encounter tables from Location or a searched place, including time/season and conditional tables where mapped. These are reference lists; scripted events and slot probabilities are not included.

- Location pages now open the reference map without enabling a permanent companion window. Supported games show mapped place names and a player-block or area/exit marker where verified data is available.
- Correct live Gen4 map IDs so the displayed coordinates and map refer to the same location.

- Search companion reference data for Pokémon, items, moves, places and trainer teams. Names, move values and trainer teams come from the loaded game; results can include spoilers. Search supports typing or selecting letters with a controller.
- Expand authored, reveal-on-request guide hints to 37 per HGSS version, 23 for Platinum and 12 for Black, including field-move locations and common story obstacles.

- Correct Platinum battle-scene detection and reject empty battlers. Correct Black's Pokédex ability fields, including hidden abilities; companion Pokédex pages also show base stats and abilities.

- The companion Position tab can show a full reference region map read from supported DS games. Showing it is explicit because it includes places not yet visited; unlock progress is not applied. Black also shows mapped place names.

- Bag entries now open descriptions read from the supported game's own text, with current bag quantities and party holders. Item use stays in the game.

- Companion Pokédex and guide navigation now opens each new entry at the top, so a previous entry's text page cannot hide its introduction.

- Evolution conditions are also shown in the optional companion Pokédex, using the same game data as the pause menu.

- The optional companion display now browses bag pockets, shows stack and item totals, and identifies empty pockets. Use the bottom arrows to change pockets and the text paging control for longer lists.

- Touching party rows, dated save slots and other rows with informational values opens them correctly, matching keyboard/controller selection.

- Optional Features in the launcher and pause menu lets you enable live companion updates, quick-action shortcuts, save-state thumbnails, recent-play history, second-screen artwork and GBA mirroring individually. These extras start off; existing display choices and save protections are retained.
- Save-state previews are hidden when they are older than the state, so turning thumbnails off cannot leave a misleading old picture.

- Add read-only companion party summaries, bag pockets and Pokédex progress/details for verified English DS profiles. Names and reference data are read from the player's own ROM; other revisions show an unsupported notice.

- Android DS games: optional second-display touchscreen, with merged gameplay or separate DS top/bottom screens. The normal layout is retained when no second display is available. Hardware validation is pending.

- TYPE MATCHUPS in the DS pause menu shows attack effectiveness against one or two manually selected types, adapted from PokeDaisy. It uses the original Gen 3-5 chart; abilities, items and move-specific exceptions are not included.

- Android launcher defaults to landscape with rotation locked, follows device orientation with auto-rotate enabled, and uses larger settings text in both orientations with wrapped values.

- Android DS/GBA FPS counters sit inward from screen edges to avoid rounded corners and camera cutouts.

- Combine French/Spanish regional profiles and Italian HeartGold, with separate save names for each language. The interface has one AUTO/English/Spanish/Italian language setting; translated pause menus expand to fit their labels. Integrated from PRs #41, #18 and #9.

- Android touch controls can be moved and resized in a paused layout editor, with save, cancel and reset. Layouts are stored separately for DS/GBA and each screen orientation.

- **LANGUAGE in the settings (DISPLAY): English or Spanish** for the game list, the settings page and the pause menu; Spanish's accented letters drawn in the menus' font.
- Android: build both native libraries for 16 KB page-size compatibility, restore launcher textures after graphics-surface resets, fall back to software drawing if the graphics driver fails, and record early native loading in startup.log.

- Mac: DESKTOP SHORTCUTS creates a game app on the desktop and in Applications, with its own picture. Adapted from HSolis95's PR #1; the existing Fulldex app and save folders are retained.

- Fix the iPhone and iPad launcher build so the hotfix can be packaged for iOS.

- Keep the established Windows renderer and frame clock by default. The recovered Direct3D 11 display-pacing path measured about 53 FPS on a 240 Hz display where the earlier path sustained about 60 FPS; it remains available with SSR_DISPLAY_PACING=1 for further testing.

- Cheat search covers names, folders and descriptions, with keyboard, touch and controller entry. Open a cheat for its full scrollable details and an explicit enabled switch; left/right still toggles it from the list. DATABASE INFO explains what was loaded.
- DS SAVE BACKUPS in the pause menu lists dated backups. Restoring requires confirmation, keeps the previous save, and restarts the game. Loading a quick state now explains how a later in-game save can replace newer progress.
- Updates compare the binary build ID in SHA256SUMS.txt, removing the 12-hour blind spot. Packages write matching build metadata, and the update popup identifies the current and available builds.

- Use the ARM64 Linux download when updating an ARM64 build, and remove obsolete Switch updater thread cleanup.

- Restore the later Windows/Linux display-pacing fix (#17), GBA RetroAchievements (#6) and ARM64 Linux packaging (#19) from their separate working copy.
- Check for updates when the launcher opens, offer INSTALL or LATER, verify downloads against SHA256SUMS.txt and retain the previous program. Mac and iOS offer the download page.

- Restore the familiar centred game grid, pixel font and picture selection frame while the new launcher design is unfinished.

- **A move with a gap in its name can be chosen with the D-pad** (SoulSilver and the other
  DS games): Rock Polish and the like were skipped because the space between the words sat on
  the one pixel the menu checked. Touch was already fine (#38).
- **Switch: HOME Menu titles spell Pokémon** with the accent, as the other Pokémon games on
  the Switch do. Choose HOME MENU ICONS again to update an icon that is already there (#8).
- **The in-game clock catches up after the phone sleeps** (Android, iOS): SoulSilver's
  Pokégear stayed hours behind overnight with the app still open; force-closing was the only
  fix. Waking the app now sets the DS clock from the phone again, as a launch already does (#36).
- **LOAD STATE no longer replaces your in-game save** with the older one inside the save state:
  loading a state from hours ago, then quitting, could put the game back there (#33). It still
  takes you back to where the state was made.
- **Backups of your saves:** the last 8 versions of each, in a `backups` folder beside your
  saves, made as a game starts and every half hour of play.
- **The sparkles on the water no longer strobe** (Black and White, with 60 FPS on): every other
  frame showed them as broken crescents; they now twinkle as the game draws them (#7).
- **No more white lines across the widescreen view in the snow** (HeartGold and SoulSilver, Mt.
  Silver): a snowflake at the edge of the DS screen was drawn out to the edge of the view (#10).
  The snow now falls in the sides of the wide view as well, as it does in Platinum and Black,
  and rain and sand do the same.
- **No dark dot on the Fight button** (Platinum, SoulSilver and HeartGold): one pixel that was not the button's flat colour was stretched across it, and showed just left of the icon.
- **Moves that cross the battle stay in one piece** (Platinum, SoulSilver and HeartGold): Flamethrower and the other moves that run from one Pokémon to the other are lengthened evenly, so the stream still reaches and is not cut across the middle.
- **GBA games on a Mac, iPhone and iPad.** Emerald, FireRed, LeafGreen and Ultra Violet show in the game list with the DS games. Put the ROMs in the same `roms` folder. On an iPhone or iPad, a ROM opened from the Files app is taken in as well, and the on-screen buttons are there while you play (#25).
- **Cheats are listed in their folders.** A cheat database no longer puts every code in one long list with the folder's name cut off the front, which made the rows unreadable. CHEATS opens the folders; a folder shows how many of its codes are on. ON NOW lists the ones that are on. The list is wider, so a code's own name fits (#32).
- **Save state and load state can be given their own key or button.** In KEYS AND BUTTONS, on the game list and in the pause menu, next to fast forward and swapping the screens. On the keyboard they start as F5 and F8. On a controller they start unset, until you choose a button.
- **Fast forward no longer rushes the music.** While it is held the game still runs ahead; the music keeps its normal speed, and lines up with the game again when you let go. A battle's music starts when the battle does, still at normal speed.
- Switch Android gameplay explicitly to landscape for DS and GBA, restoring portrait on return to the launcher.
- **iPhone and iPad: the game list is upright, and a game turns the screen sideways.** The same on a phone or a tablet. The list was sideways on both, so an iPad never used the taller layout.

- Report cartridge save failures on screen, log successful save writes, and flush Linux save data and directory changes to storage. Steam Deck rollback investigation remains open.
- Keep the SoulSilver overworld through a door or Dark Cave fade instead of sticking on the X menu / touch screen (#5).
- Show black at once when a Switch ROM starts, instead of the uninitialised white swapchain (#20).
- **Dark caves no longer go completely black** (HeartGold and SoulSilver, Dark Cave before you
  have Flash) with the world drawn by the graphics chip (PC, Switch): the circle of light around
  you shows again. The white flash of a cave's entrance fills the whole widescreen view instead of
  showing black bars at the sides for a second.
- **A move's power, accuracy and description show on the summary's moves page** (HeartGold and
  SoulSilver): choosing a move adds them beside the stats and the list. On a wide view the four
  panels shrink into one row; on a narrow view they sit in two rows (#11).
- **HeartGold and SoulSilver: the Pokédex's cover no longer shows twice** while it slides open or
  shut (#12). In the wide view the closed cover fills the screen, its buttons bar runs edge to
  edge, and it slides across the whole screen from the right edge to the left as it opens.
  On a touch screen, swipe the closed cover to the left to open it, and swipe the list to the
  right to close it.
- **Platinum: the Pokédex list sits beside SEARCH, SWITCH and CHECK**, without the Poké Ball that
  a click was turning. B QUIT sits in the header above those buttons. On a controller the face
  buttons that press them are named on each button, as that pad prints them (Xbox, PlayStation
  or Nintendo), and only while a controller is in use. On a phone they stay hidden until a pad
  is used, so the on-screen buttons are not covered.
- **Black, White, Black 2 and White 2: the Pokédex opens on its red Poké Ball cover.** In the
  wide view the ball is scaled so the red that met the DS's left and right edges meets the
  edges of the screen, instead of a small grey screen and the ball stacked or side by side.
- **Android: the on-screen SELECT and START are at the top, beside MENU**, so they no longer
  cover the game's buttons along the bottom of the screen.
- **Windows: a game started from the game list takes the keyboard and controller at once**; until
  its window was clicked, they did nothing.

- Android launcher opens in portrait, with bounded rendering resolution and remembered console tab.
- Check ROM banner and artwork bounds, skip repeated ROM paths and reduce background scanning.
- Back up existing saves before launching and report missing ROMs or unwritable save folders.
- Write launcher settings through a temporary file and fail Android builds on Java errors.
- Check official GitHub releases from Settings > Help and offer the Android APK download.


- **Pokémon Diamond and Pearl (experimental):** in the game list, with Platinum's one-screen
  layouts: their functions found from Platinum's (`tools/port_family.py`), in English and
  Diamond in Spanish. Not yet played through.
- **Spanish versions:** Pokémon White (Edición Blanca), Platinum (Platino), SoulSilver (Plata
  SoulSilver) and HeartGold (Oro HeartGold) in Spanish play with the one-screen view, as the
  English ones do. Another language's tables can be made the same way, from that language's
  ROM and the English one (`tools/port_version.py`).
- French revision 0 ROMs of Black, White, Black 2, White 2, SoulSilver, HeartGold
  and Platinum, with separate French saves and regional widescreen map profiles.
- Platinum: controller answers work with the French introduction's OUI/NON buttons.
- **A new address: github.com/mamesj/pokemon-fulldex** (the old pokemon-ds-widescreen address
  still leads there). The bug report link, the PC download's `README.txt` and the Linux
  AppImage's updates use it.
- **The README:** a new trailer (the new name, the GBA games, RetroAchievements, cheats and the
  game list's look), and pictures of the GBA games: Emerald's Rayquaza, Pacifidlog and
  Sootopolis, FireRed's Mewtwo and Snorlax, LeafGreen's Articuno and Lavender Town.
- **The GBA games take the DS games' settings:** SMOOTH SCALING, FPS COUNTER, VOLUME and BUTTONS
  are copied to them each time one starts, so they look and sound the same. With SMOOTH SCALING
  off, the GBA's own SCALING stays SHARP (fills the screen) or WHOLE PIXELS (the crispest, with a
  border; its WIDESCREEN 18:9 fills the width of a 720p screen with it).
- **RetroAchievements: an ACHIEVEMENTS list** in the pause menu (RETROACHIEVEMENTS > ACHIEVEMENTS,
  once a game's set is loaded): every achievement with its points or DONE, unlocked ones first;
  the chosen one's description, points and progress so far under the box. A on one opens its
  details: the whole title and description, points, when you unlocked it (or progress so far) and
  the share of players who have it.
- **RetroAchievements from the game list too** (PC, Android, Mac, iPhone and iPad): SETTINGS >
  RETROACHIEVEMENTS turns it on and LOGS IN (or out) before you start a game; the games use the
  same account. The pause menu's RETROACHIEVEMENTS is still there.
- **The game list's own look: GAME LIST on its settings page.** COLOUR changes its lines, the frame
  round the chosen game and the tabs (red, blue, green, purple, gold, pink or teal), and the pause
  menu's colours in the games too (BLUE is the pause menu as it was). BACKGROUND:
  YOUR PICTURE shows a picture of yours behind the games, darkened so the names stay readable: name
  it `background.jpg` or `background.png` and put it beside your `saves` folder (on the Switch, in
  `/switch/pokemon-ds-wide/` with the saves). Best at 1920 x 1080; any size fills the screen.
- **The game list's title reads POKÉMON FULLDEX**, with its É.
- **A new name: Pokémon Fulldex**, now that it plays GBA games as well as DS ones. The app, the
  Switch's Homebrew Menu, the phone icons and the PC's window all show it; on Windows the program
  is `Pokemon Fulldex.exe` (DESKTOP SHORTCUTS: CREATE again for shortcuts to it). Nothing moves:
  the ROMs, saves, settings and the downloads' file names are where and what they were.
- **SOUND, with VOLUME**, a page of its own in the pause menu and on the game list's settings
  page: a slider from OFF, then 10% to 100% (left and right change it, or tap the bar on a
  phone).
- **RetroAchievements on every platform:** RETROACHIEVEMENTS in the pause menu (Windows, Linux,
  Mac, Android, iPhone and iPad, Switch). Turn it on, LOG IN with your RetroAchievements account,
  and unlocks pop up as you play. Casual mode: save states, fast forward and cheats still work
  (achievements are paused while a cheat or OTHER VERSION POKEMON is on).
- **The game list opens on ALL, with a heading for each console (DS, GBA, 3DS)**, and tabs along
  the top to show one console's games: L and R on the Switch, Q and E (or a controller's shoulder
  buttons) on a PC, or a tap on the tab on a phone. The 3DS tab has Pokémon Omega Ruby, COMING SOON.
  On a phone the list scrolls with a finger (on a PC, with the mouse held down too).
- **GBA games: Pokémon Emerald, FireRed, LeafGreen and Ultra Violet** in the same list, in
  widescreen at 60 fps (Pokémon GBA Widescreen, built in). Put the ROMs in the same `roms` folder
  (FireRed and LeafGreen version 1.0 or 1.1; Ultra Violet's `.gba`, or its `.ips` with a FireRed
  1.0 ROM beside it); their saves go with the others (`emerald.sav` ...). A `GBA` folder inside
  `roms` works too, with the saves in its own `saves` folder (as Pokémon GBA Widescreen keeps them
  on a phone, so a game carries on). Other GBA games play
  too, without widescreen. QUIT GAME in a GBA game's pause menu comes back to the list.
- **Android: a fast forward button on the touch controls.** Hold the >> button beside R at the top
  of the screen (up to 4x). R itself stays the DS's R button; with a controller, fast forward is
  still its right trigger.
- **Windows: the taskbar shows the program's own icon.** A desktop shortcut to one game that was
  pinned (Platinum's, say) took over every window of the program, the game list's included, with
  that game's picture; the program and each game's shortcut now have their own place on the
  taskbar (SETTINGS, SHORTCUTS, DESKTOP SHORTCUTS: CREATE again updates ones made before).
- **No version exclusives (OTHER VERSION POKEMON in the pause menu): SoulSilver, HeartGold,
  Black, White, Black 2 and White 2.** Where the two versions' wild Pokémon differ, each encounter
  can be either version's, as often as each other, at that place's own rates and levels (Gothita
  or Solosis, Cottonee or Petilil, Vullaby or Rufflet, Phanpy or Teddiursa...). Legendary Pokémon
  stay with their own version. Off to start; RetroAchievements are paused while it is on.
- **SoulSilver, HeartGold and Platinum: the European English copies** are listed as working: their
  code is the US versions' (only a picture and the version number differ), and they play the
  same, with the one-screen layouts and the widescreen world.
- **Black 2: the sky from Aspertia City's lookout** reaches both edges of the wide view: the haze
  over the sky and mountains no longer stops short on the right (a darker block in the top right
  corner, and the mountains beyond it in other colours), and the sky's left panel no longer leaves
  a paler wedge in the top left corner.
- **Black 2: no flicker at the edges as the camera rises at Aspertia City's lookout.** The sky,
  hills and mountains were continued to the wide view's edges in some frames and not others while
  the view tilted up. A wedge of grass that flashed over the cliff at the top right, for a frame,
  is gone too: the far hillside was stretched past where it really ends. Below the lookout, the
  fence round the planter by the street no longer runs on across the street as a long rail, and
  the paving beside it has no jagged step. Walking up the stairs, the cliff on the right no longer
  flickers as a stretched slab and the tree beside it no longer smears sideways: near things
  seen from above are no longer taken for the distance.
- **Black, White, Black 2 and White 2: no flicker at the sides of the wide view as you walk.** On
  the stairs up to Aspertia City's lookout the ground, a tree and the wall on the right flickered:
  near things were stretched out to the view's edge in some frames and not others. Only the
  distance (sky, clouds, haze, mountains) is now continued to the edges, and what continues it
  changes only once it has held for a few frames. From the lookout, the mountains on the left no
  longer stretch as you walk from side to side (their picture repeats on instead), and the white
  mountain on the right no longer stops in a slanted cut with the sky beyond it.
- **Switch: the game list's settings no longer seem to freeze.** Y opened the settings page, but
  the game list was drawn over it, so the buttons went to a page you could not see. The list also
  has a SETTINGS button at the top right, as on a PC or a phone: tap it in handheld mode, or press
  up from the tabs and then A; < BACK at the top left of the settings goes back.
- **Game list: the tabs with the D-pad or arrow keys.** Up from the top row of games chooses the
  tabs (ALL, DS, GBA, 3DS); left and right then change the tab, down goes back into its games, and
  up again chooses SETTINGS. L and R (Q and E on a keyboard) still change the tab from anywhere.
- **Widescreen: things no longer vanish while still on screen.** Black 2's rocks and lamp posts at
  the bottom of the stairs up to Aspertia City's lookout disappeared a few pixels before they had
  scrolled out of the picture, and appeared late; the game now draws what is near the picture's
  edges too (all the games, with the wide view on).
- **New: iPhone and iPad.** A sideloaded app (`pokemon-ds-wide-ios.ipa`, iOS 14 or newer): the
  same game list and games as on Android, with the ROMs, saves and settings in the Files app
  (On My iPhone > Pokemon DS Widescreen).
- **HeartGold in Italian** (Pokémon Versione Oro HeartGold) plays with all its one-screen layouts,
  as the US copy does; its save is `heartgold-it.sav`.
- **KEYS AND BUTTONS in the game list too:** its settings page (CONTROLS) changes the keys and
  buttons before you play, as the pause menu does; on the Switch as well (any controller it
  takes, sys-con and MissionControl ones included), and its list follows the buttons changed.
- **Cheats: CHEATS in the pause menu.** Action Replay codes, run by melonDS's own engine, from a
  `cheats` folder: the R4 / DeSmuME database (`usrcheat.dat`), a text file of your own codes named
  after the game's code (`IPKE.txt` for HeartGold, ...) or a melonDS `.mch` file. Each turns on and
  off as you play, and stays as you left it. Long lists scroll, and the chosen cheat's note from
  the database (and its whole name, when the menu has to shorten it) shows under the menu.
- **SoulSilver and HeartGold: the Running Shoes button beside the X menu.** The game greys it out
  while its menu is open, and the single-screen view only shows the touch screen then; now the
  button stands next to the menu, ON or OFF, and Y (or a touch on it) turns running on or off.
- **SoulSilver and HeartGold: the title screen on both screens**, the logo beside the legendary
  bird, as the other games show theirs; the opening movie shows its two screens one above the
  other (the moon and the sea were on the hidden one).
- **README: a trailer at the top**, every game on every platform.
- **The pause menu and the settings page in groups:** the pause menu is RESUME, SAVE STATE, LOAD
  STATE, FAST FORWARD, then DISPLAY, GRAPHICS and CONTROLS (each a page of its own), RESET GAME and
  QUIT; the game list's settings page opens on DISPLAY, GRAPHICS, CONTROLS, SHORTCUTS and HELP.
- **Game list: SETTINGS can be chosen with the D-pad** (up from the top row, then A), as well as
  with its button; on the settings page, up from the top reaches BACK.
- **Button prompts name your controller's buttons**: PlayStation's symbols (cross, circle, square,
  triangle), Nintendo's and Xbox's, and the buttons you gave in KEYS AND BUTTONS.
- **Remappable controls: CONTROLS > KEYS AND BUTTONS in the pause menu.** Every DS button, the pause menu, fast
  forward and swapping the screens can be given another key on the keyboard and another button
  on a controller (triggers too). Choose one and press the key or button for it; giving it one
  that something else had swaps the two, and RESET ALL puts everything back. On the PC, Mac and
  Android game list, A, B, Y and the directions follow the ones you change.
- **Mac: Pokemon DS Widescreen.app** for Apple silicon and Intel Macs (macOS 11 or newer), built on
  GitHub's Macs (`tools/mac/build_all.sh`, `.github/workflows/mac.yml`). Its ROMs, saves and
  settings go in `~/Library/Application Support/Pokemon DS Widescreen`; the world is drawn by the
  processor (a Mac's OpenGL stops at 4.1), at 720P to start; a MacBook Air M1 holds 60 FPS at
  1080P. Not notarised: the first time, Open Anyway in System Settings > Privacy & Security.
- **Linux: an AppImage** (`pokemon-ds-wide-pc-linux-x86_64.AppImage`), the program as one file, for
  Gear Lever and other AppImage managers, which also update it from the release. Its ROMs, saves
  and settings go in `~/.local/share/pokemon-ds-wide` (O in the game list opens its roms folder);
  its desktop shortcuts start the AppImage.
- **Switch: HOME MENU ICONS could fail with THIS BUILD HAS NO FORWARDER** when the game list had no
  game that needed the program's built-in tables (only SoulSilver and HeartGold, or games with their
  own funcs.txt). Fixed.
- **LOWER DETAIL WHEN BUSY (LOWER RES WHEN BUSY on a PC or phone) is off to start**, on every
  platform, also for players who had it on from before; turn it on in the settings if you want it.
- **Android: the first public release** (`pokemon-ds-wide-android.apk`), all seven games in one
  app, with on-screen controls and controller support. README: an Android section.
- **START opens the pause menu** (+ on the Switch, a controller's START on a PC or phone). The
  games hardly use the DS's START; START BUTTON in the settings puts it back (then + and − together,
  or START and SELECT, open the menu as before).
- **Android: a proper app icon** that fills the launcher's circle, instead of a square picture
  inside a white circle.
- **README: Use your own save**, near the top: where each platform keeps the saves, each game's
  save name, and which save files work.
- **One download per platform:** the all-in-one for the Switch and the PC download, each with all
  seven games. The one-game Switch downloads are no longer published; their folders keep working
  from the all-in-one's list.
- **Switch: the touch screen works** in handheld mode. Taps did not reach the game, so
  questions answered only by touch (Platinum's first YES / NO with Professor Rowan) could not be
  answered. Docked (or handheld), the right stick moves a cursor and ZL touches; pressing the
  right stick in shows the other screen.
- **Platinum: the introduction on one screen.** Professor Rowan's screen fills the view; for his
  two questions that take a touch, the YES / NO buttons or the Poke Ball stand at the right edge
  (touchable). A and B answer them too (A for YES, B for NO, A presses the Poke Ball's button),
  which the game itself only takes as a touch.
- **Switch: HOME MENU ICONS** in the all-in-one's settings (Y in the list): an icon on the HOME
  Menu for each game, with its box art, that starts the game straight away. Made on the console
  (no PC or keys file); like any forwarder they need sigpatches. The list shows box art for every
  game too.
- **PC: desktop shortcuts for each game**: DESKTOP SHORTCUTS on the game list's settings page
  makes them, with the box art as their icons; each starts its game without the list
  (`--game IREO` on a command line does the same).
- **PC: FAST FORWARD LOW RES**, a new setting, off to start: your 3D resolution now stays while
  you fast forward; turn it on for more FPS in fast forward (the world at 360P while it is held).
- **PC: box art in the game list**, built into the program, as on the Switch (your own pictures in
  `icons/` still take its place).
- **Android: one app for all seven games** with the game list,
  built by `tools/android/build_all.sh`. The first time, tap the screen, allow access to files
  and pick the folder with your ROMs (saves go in a `saves` folder inside it). On touch screens
  the game list, its settings page and the pause menu take taps (drag to scroll the settings), and
  the on-screen D-pad has a dark edge so that it shows on light scenery.
- **Settings in the game list**: press Y (on a PC also H or F1) to set the pause menu's options
  before you play, each with what it does written under it.
- **Switch: a shorter pause menu.** WORLD ON GPU and STICK TOUCH CURSOR are gone from it: the
  world is always drawn by the graphics chip (if it ever fails to keep going, the processor
  draws it for that start only), and the Switch has its touch screen.
- **PC: the Linux download brings its own SDL2**, so nothing needs installing first, and it
  runs on the Steam Deck (SteamOS): see the Steam Deck steps in the README.
- **PC: a first start in borderless fullscreen at 1080P with 60 FPS on** (F11 or the settings
  for a window; LOWER RES WHEN BUSY brings the 3D resolution down on a PC that cannot keep up).
- **PC: the Windows program is now `Pokemon DS Launcher.exe`**, with its own icon, and the
  menu's FULLSCREEN is called BORDERLESS FULLSCREEN (which is what it always was) and WORLD DETAIL
  is 3D RESOLUTION, shown as 720P, 1080P, 1440P (2K) or 2160P (4K). The game list
  can show your own pictures of the games: see `icons/` in the PC README.
- **Fast forward is on the right trigger in every game** (ZR on the Switch, RT on a PC
  controller): hold it for up to 4x speed. It was the left trigger. With STICK TOUCH CURSOR on (PC),
  the left trigger now presses the touch screen instead of the right.
- **Fast forward goes faster and looks smoother.** Of the frames it runs at once only the last
  is shown, so the others are no longer drawn (their 2D and widescreen world); the 60 FPS
  in-between frames pause and the widescreen world is drawn at 360 lines while it is held (back
  to your WORLD DETAIL as soon as you let go); and where the console cannot run 4 frames at once
  any faster than 3 (the Switch), it runs 3 and shows more of them.
- **Switch: fixed a crash when LOWER DETAIL WHEN BUSY brought the world down to 423 lines**
  (in heavy scenes, and nearly always while fast forwarding): at that size the world is now
  drawn the way it is at the other sizes the detail goes through.
- **Switch: smoother in heavy scenes.** The game now runs one frame ahead of what is on screen
  and shows each frame at an even time, so a frame that takes a little too long no longer
  makes its picture late when the frames around it are quick. At a 1581 MHz CPU clock, frames
  shown unevenly (over 17.5 ms after the last): White 2's Aspertia City from 39% to 17%, a
  Black 2 battle from 29% to 19%, Platinum's Snowpoint City from 30% to 10%, a HeartGold route from 19% to 2%. The cost is one
  more frame (17 ms) between pressing a button and seeing it, which in these turn-based games
  is hard to notice.
- **Switch: fewer late frames in towns and battles.** Preparing the 3D pictures for the
  graphics chip, sending them and collecting them now happens on the Switch's spare cores
  instead of between the game's own work, about 1 ms less a frame. Measured from save states,
  frames running late: White 2's Aspertia City from 4.5% to 0.9%, Black 2's Castelia City from
  1.6% to 0.1%, a Black 2 battle from 6% to 2.5%, Platinum's Snowpoint City from 4.8% to 1.7%.
  The picture is the same as before.
- **Switch: a game picked from the game list runs as smoothly as one started on its own.** The
  list left the Switch's graphics slower for the game played after it (HeartGold took 12 ms a
  frame to put its picture together instead of 4, and one frame in ten ran late). The program
  now starts itself again for the game you pick (a second of black screen, as QUIT already
  has), and the game runs at full speed.
- **Switch: smoother in towns, 60 FPS where it dropped to the mid-50s** (White 2's Aspertia City,
  Black 2's Castelia City). Where a game draws its 3D every other frame, each new 3D frame
  meant building the wide view's polygon lists and the 60 FPS in-between frames, about 8 ms of
  emulation every second frame. That work now runs on a spare core of the Switch, and the
  in-between frame's matching of polygons is worked out once instead of twice. Measured from
  save states: Aspertia from 56 to 60 FPS (frames running late from half to 4%), Castelia
  walking from 35% to under 2% late. The picture is the same as before.
- **Switch: fixed a rare freeze a few seconds into a game** (black screen, only holding POWER
  got out of it). The game asked the console for its clock speeds and temperatures for its log,
  and with sys-clk installed that could crash the console's power service. A player's log now
  goes without them.
- **Switch: smoother battles while you choose a command** (SoulSilver, HeartGold, Platinum):
  the buttons beside the battle are only sent to the graphics chip when they change, instead of
  every frame; and the chosen button has its white outline on the Switch too.
- **SoulSilver, HeartGold and Platinum battles:** the message box beside the buttons ends
  cleanly again on the Switch (its closing edge was drawn the wrong way round, a dark line
  beside the text); the party's Poke Balls sit in two straight rows with room between them,
  without the grey ring around each.
- **A new look for the game list** (Switch and PC): black with white text and red details, and
  four games across when there are more than six, so all seven fit on one screen.
- **PC all-games download (new, Windows and Linux):** unpack it, put your ROMs in its roms
  folder and pick a game from the list (keyboard, controller or mouse); QUIT in the menu goes
  back to the list. Saves go in its saves folder, one per game.
- **PC: the world on the graphics card, up to 4K.** WORLD ON GPU (on by default in the PC
  download) draws the widescreen world with the graphics card instead of the processor, and
  WORLD DETAIL now goes up to 2160 lines (4K).
- **Platinum battles:** "Use next Pokemon?" after a faint shows its two buttons (Use next
  Pokemon and Flee) instead of a cut-off red bar.
- **Black, White, Black 2 and White 2 (playtest notes):** no more trees drawn as thin slivers at
  the edge of the wide view (Castelia City's plaza); calm side bars instead of streaks while
  a door's zoom, a key item's cut-in or Fly covers the world; Black and White's start menu
  with no saved game shows its list properly; the X menu no longer opens as a grid of black
  boxes; going back from the Bag to the X menu goes straight to the wide view.
- **SoulSilver and HeartGold menus:** the Bag shows the chosen item's description above the
  items; no black box where the X menu was while it closes into the save or a menu; the
  Pokemon summary no longer flashes empty frames while it fades; a fade to white (choosing
  your starter, whiting out) fills the whole view instead of a white square with black bars.
- **SoulSilver and HeartGold: smoother while a battle waits for your command.** The battle
  buttons were drawn from hundreds of small pieces, each sent to the Switch's graphics chip
  every frame; each button is now a few pieces.
- **No more stretched trees at the edges of the wide view:** a tree, house or flower bed that
  ended just short of the screen's edge was sometimes stretched out to it (SoulSilver and
  HeartGold by New Bark Town, Black 2). It now stays its own size, and SoulSilver, HeartGold and
  Platinum no longer have anything continued to the edges at all (only the Black games' skies
  and lookouts need it).
- **No more dotted black lines in the wide world (WORLD ON GPU):** pixels the graphics chip left
  out where two polygons meet (along the ground's seams in SoulSilver and HeartGold) are filled
  from beside them.
- **Switch: the log of the game you just played is kept.** QUIT back to the game list used to
  replace `game.log` with the list's few lines; the previous one is now kept as
  `game-previous.log`.
- **Battle buttons are easier to read:** the chosen command or move has a thick white
  outline and the others are shaded, so you can always see what A will press (the red outline
  was lost around the red FIGHT button).
- **One program for all six games (Switch):** the all-games download
  (`pokemon-ds-wide-all-switch.zip`) is now a single program that shows a grid of your games when it starts, each with the icon from its ROM (or
  your own picture in `icons/`, or the HOME Menu icon of a one-game program beside it), and
  **QUIT** in the pause menu goes back to it. It also lists the games in the other
  `/switch/` folders (the one-game downloads), each played with the save in its folder, so
  nothing needs moving. The download comes with an empty `roms` folder for your ROMs. The one-game downloads stay as they are.
- **Black and White battles: catching and levelling up.** A thrown Poke Ball is shown whole
  as it lands and shakes (it was cut in two, its other half at the view's right edge: it lands
  where the player's HP box is, whose place was still being moved while the box was put away),
  and the level-up window with the new stats moves with the player's box as one piece instead
  of being cut through (Black 2 already did this; Black's own box places overrode it).
- **Black and White: the wide view's edges in towns and on routes** (trees popping in and
  stretching at the sides, as in Nuvema Town) load the wider map as Black 2 and White 2 do:
  the map's pieces are loaded further out, with the extra memory of the DSi mode
  (`bw2map.h` now knows Black's and White's addresses). Save states made before this need
  `SSR_B2_MAP=0`.
- **Black and White's bag with an empty pocket** is shown as the bag screen alone, without an
  empty card beside it ("It's empty." with nothing to describe).
- **WIDE MAP LOADING (pause menu, Black, White, Black 2, White 2):** ON (the default) loads
  more of the map around the player so the wide view's sides are not empty; OFF leaves the
  game's own map loading alone, if it ever causes trouble. The game sets its memory up for it
  as it starts, so a change takes effect on the next start (the menu says "NEXT START" until
  then). Kept in the settings as `wide_map`.
- **Pause menu:** no game name at the top (it said "POKEMON SOULSILVER" in every game).
- **Trainer battles' start (Black 2, White 2, Black, White): the bars of Poke Balls** that slide
  in for each side's party are shown whole: the HP boxes' places stay put while they are up,
  instead of the bars being cut where the boxes' pieces are moved to the view's edges.
- **Start menus redone for the wide screen.** Black 2, White 2, Black and White: the list and
  the saved game's cards side by side across the whole view on the menu's dark grey, the
  buttons and cards without the Poke Ball picture behind them (it showed cut up and blended
  into them), the lower card closed off cleanly, and the whole list always shown with the
  highlight moving down it (Black's list scrolled the card away). SoulSilver and HeartGold:
  the touch screen's menu at full height with its blue carried to the edges instead of black
  bars. The screens are drawn again from the game's backgrounds for this
  (`belowscreen.h`, `Scene::redrawSkip`, `Scene::redrawUnscroll`).
- **The opening movie of Black and White (and Black 2 and White 2) has plain black bars** beside
  it instead of streaks of its edge colours.
- **Black and White's Pokédex list:** the SEEN / OBTAINED counts above the list, large,
  instead of both screens as a thin column.
- **SoulSilver and HeartGold: SAVE** shows its screen whole from the start (for a second or
  two it was cut to the start menu's panel at the right edge).
- **Switch: new names, without "ssrecomp".** The games' folders and programs are now
  `/switch/pokemon-<game>-wide/pokemon-<game>-wide.nro` (black2, white2, black, white,
  soulsilver, heartgold) and the downloads `pokemon-…-wide-switch.zip`. The settings file beside
  the game is `settings.cfg` (an old `ssrecomp.cfg` there is renamed to it), and the program's
  messages say "Pokemon DS widescreen" and mention starting it from a forwarder.
- **Pokémon White (English, US/Europe).** The same program and layouts as Black: White is the
  same game with its code a few bytes apart, so its function table is Black's at White's
  addresses (`tools/bw1/white_shifts.txt`, found by `tools/bw1/match_white.py`;
  `tools/bw1/funcmap.py --white` makes the table). On the Switch it gets `white.sav`, and the
  messages for a missing or unknown game list it.
- **Black: double, triple and rotation battles laid out for the wide view.** The foes' HP boxes
  stay down the left edge of the view and yours down the right edge, where Black draws them
  (they were cut by the single battle's places), and the battle buttons sit in the message
  box's band so they don't cover your boxes. Choosing a move's target shows the target buttons
  there too (it showed none). Black's table now also knows the target menu, the level-up
  window and the evolution scene by Black 2's names. Headless tests: `--state-at` counts from
  the loaded state under `--relative`, like the other frame options.
- **SoulSilver and HeartGold (English, US) as a download, on the fast JIT.** The program holds
  none of the games' code: its tables have addresses only, and the overlay code the game
  watch compares is read from your ROM when the game starts. HeartGold is told apart by its
  game code; its few moved functions come from `tools/soulsilver/heartgold-us.json`. All of
  SoulSilver's layouts work without the recompiled code, about 1.7 times as fast. The new
  game's first menu is skipped again (also in HeartGold). Battles' widened picture is made
  beside the next frame's emulation. A bot for long test runs: `tools/soulsilver/bot/`.
  On the Switch, HeartGold gets `heartgold.sav` and the JIT like SoulSilver, and the message
  for a game the program doesn't know lists both.
- **The bars beside an evolution or a battle take the screen's outermost colour that isn't
  black** (from its last four columns), so a scene that leaves a black column at the screen's
  edge (SoulSilver's evolution) no longer gets a black bar on that side.
- **3D drawing commands are no longer lost (Black 2, White 2 and the other games on the fast
  JIT).** When the console's 3D command queue was full, a burst of commands from the fast JIT
  could overflow the small overflow store melonDS keeps, and from then on every command read
  its neighbour's numbers. Fixed: White 2's overworld not drawn after a trainer battle (so
  White 2 now has the wider map too), triple and double battles' backgrounds going black for
  a few seconds after a Pokémon faints, and the summary screen's Pokémon sometimes missing.
  Save states made before keep working and fall back in step at once.
- **Black 2 and White 2: double, triple and rotation battles' HP boxes against the edges of the
  wide view,** the foes' in the upper left and yours in the lower right, as in single battles
  (they kept the DS's places in the middle). The boxes stay put while the screen goes black
  into an evolution, instead of the next scene showing through where they were.
- **Switch: a shorter pause menu.** The game always shows the one wide screen, so SCREEN LAYOUT
  and LARGE SCREEN are gone (an older saved layout is ignored). CPU BOOST is gone: the system
  kept taking its boost mode away and the game dropped frames; sys-clk sets the clocks
  instead (see README). EMULATION is gone: FAST JIT always, still falling back by itself for
  one start if a start with it fails, and the interpreter while L is held. BUTTONS, which
  had nothing to choose on the Switch, is gone too.
- **Pokemon White 2 (US/Europe, English):** plays like Black 2, with the same layouts, wide
  view and battle screens (its function table comes from the pokebw2 decompilation's White 2
  symbols), and the wider map.
- **Switch, Black, Black 2 and White 2 start at WORLD DETAIL 720 with 60 FPS on** (with WORLD
  ON GPU, about 16 ms a frame), instead of 288 lines and 60 FPS off, which suited the world
  drawn by the processor. Settings you have already changed are kept.
- **Switch, Black 2 and White 2 without building:** the program can carry the function tables
  for Black 2, White 2 and Black itself and read the few bytes it needs from your ROM when
  the game starts, so one ready-made `.nro` plays any English copy of those games
  (`tools/switch/build_game.sh bw`). A ROM it doesn't know (another language) gets a message
  saying which versions it plays.
- **Switch: one command per game.** `tools/switch/build_game.sh soulsilver|black2|black
  your-rom.nds` does every step once the tools are installed: the function tables, the
  build (with devkitPro's Docker image when devkitPro is not installed) and a folder laid out
  like the SD card (see README, "Nintendo Switch").
- **SoulSilver on the Switch's JIT:** built without the recompiled code
  (`tools/soulsilver/tables_only.py`), it runs on melonDS's JIT, about 1.7 times as fast on
  the Switch, and builds in minutes; its layouts learn which functions and overlays run from
  the emulator instead. In a wide battle the bottom band shows the message box on its left
  and the touch screen's buttons on its right.

- **Nintendo Switch (experimental):** a homebrew `.nro` for consoles with custom
  firmware, built on the PC from your ROM with `tools/switch/build.sh` (see README,
  "Nintendo Switch"). The pause menu's FULLSCREEN is CPU BOOST there, the
  widescreen world starts at 1.5 times the DS's sharpness (WORLD DETAIL 288) and
  60 FPS starts off. The buttons follow the Switch's own labels (A is the right
  button). Quitting saves and closes cleanly (each step is written to `game.log`).
  CPU BOOST now sets the speeds itself, as sys-clk does, instead of asking for the
  system's boost mode (which the system took away for 10-20 seconds every minute or
  so): processor 1785 MHz, memory 1600 MHz (its docked speed), checked five times a
  second, and put back to normal when it is turned off or the game ends.
  Games without recompiled code (the experimental Black 2 build) run on melonDS's
  JIT on the Switch (EMULATION in the pause menu; hold L while starting for the
  interpreter, which is also used by itself after a start where the JIT failed).

- **WORLD ON GPU (Switch, pause menu):** the widescreen world drawn by the Switch's
  graphics chip through deko3D, its own graphics API, instead of the processor, into the
  same picture. On the Skyarrow Bridge at WORLD DETAIL 720 a frame went from about 34 ms
  (30 fps) to about 16 ms (60 fps). On by default; if the game does not get 600 frames
  into it, it is off at the next start. With it, the DS's own 3D picture is drawn by the graphics chip
  too (`ds3d_gpu=0` keeps that on the processor), and the texture memory is only sent to
  the graphics chip when it has changed. On a PC the same
  drawing runs through OpenGL in headless tests (`--wide-gpu`), to compare with the
  processor's picture: they differ only along the edges of polygons.
  LOWER DETAIL WHEN BUSY works with it too: where the graphics chip spends more than 11 ms
  of each frame on the world (a battle at 720 lines), the world is drawn with fewer lines
  until it fits, and back up when there is room.
  Where the game draws its world every other frame (Black 2's cities) and the graphics
  chip has not finished a picture in time, the last one stays on show for a frame instead
  of the game waiting for it; and the graphics chip shades fewer hidden pixels.
  The graphics chip now keeps its pictures compressed and draws the world straight into
  the frame it shows, and each DS texture is decoded once (when the game changes it)
  rather than again for every pixel (that decoding was over half of its drawing time).
- SoulSilver: a new game's first menu, which is on the touch screen alone ("Please touch any
  topic you'd like to know more about"), is answered NO INFO NEEDED by itself as it comes up;
  pressing A had chosen the controls explanation again and again.
- Black 2 battles: the level-up window (the list of stats) moves with your HP box instead of
  being cut through by it; in double, triple and rotation battles the buttons sit in the
  message box's band (as BATTLE BUTTONS' PANEL), below all your HP boxes instead of over the
  lowest one, and choosing a move's target, which was not shown at all, is.
- Black 2: with the widescreen world on, the game loads more of its map beside you (3 of its
  32-tile chunks across instead of 2), so the wide view no longer comes to the end of the
  land where the DS's own screen could not see (beside the cliff at Aspertia's lookout). The
  DS has no room for that in its 4 MB, so Black 2 now gets 8 MB, as the DS's development units
  have, with the game's memory moved into the upper half (as it is given more on a DSi). Save
  states made before keep the game's own map until the game is next started.
- Black 2: where a sky or a distant layer of the view (Castelia zoomed out; the sky,
  mountains, haze and far trees from a lookout) ends a little short of the wide view's edge
  with nothing beside it, it is continued to the edge, its picture carried on, instead of
  leaving a strip of flat colour. The start menu's grey
  circle no longer runs on below the screen as a straight band.

- Switch: quitting no longer hangs when closing the sound never returns (seen at
  "[exit] audio"): after three seconds the program ends without it.
- Black 2: the world no longer flashes up for a frame between CONTINUE and the season's
  screen (the widescreen world is left out while the game shows no 3D). The start menu is
  drawn with sharp pixels and fills the height of a wide view: its list and the saved
  game's card go on below the screens, drawn from what the game has ready there, so the
  last entry is no longer cut in half.
- **Pokemon Black (experimental):** the same program runs Black (US/Europe) like Black 2, from
  a function table made from the aletheia decompilation (`tools/bw1/`), with Black's own
  layouts where its screens differ (`frontend/bw1.h`): the start menu (the saved game's card
  beside the whole list, drawn on below the screen), the X menu, single battles' HP boxes, the
  Badge Case, the Pokedex registration after a catch and the save screen (as Black 2's).
- Black 2: in a single battle on the wide view the HP boxes are back where the game has them,
  the opponent's in the upper left and yours in the lower right (just above the battle's
  buttons), each against the edge of the view, instead of both along the top.
- **BATTLE BUTTONS (pause menu, Black and Black 2):** how a battle's touch-screen buttons are
  shown beside the wide view: CORNER (the panel in the lower right corner, as before; Black 2's
  default), PANEL (the touch screen's panel as tall as the message box) or, in Black, STRIP
  (its default: the buttons large in the message box's band, the one the cursor is on framed).
- Black 2: saving from the X menu has a layout of its own. The question, YES / NO and the
  messages that follow are in a message box along the bottom of the overworld, and the
  saved game's card (with the progress of the save) is a panel in the upper right corner.
  Before, all of that was on the hidden touch screen, and while the save was being
  written both screens popped up side by side for a second or two.
- **Test runs on the Switch:** a `test.txt` in the game's folder lists runs (a snapshot to
  start from, how many frames, which frames to take pictures of, an input script, settings
  for the run only). Each start of the game does the next run, writes the pictures (BMP), a
  line of timings per frame (`timing.csv`) and its `game.log` to `test-results/<run>/`, and
  quits; it then starts again by itself for the next one. The save and the settings are left
  alone. When all are done, `test.txt` becomes `test-done.txt`. On the PC the same options
  exist: `--relative`, `--timing <file.csv>`, `--set key=value`, `--keep-settings`. See
  `frontend/testrun.h` for the format.

- **3D ON GPU** (pause menu, Linux; "3D ON GPU (TEST)" on the Switch): the 3D scene, the
  widescreen world included, is drawn by the graphics chip through OpenGL instead of the
  processor, on the thread that shows the frames. Off by default. If it was on and the game did
  not get 600 frames into it (it hung a Switch once), it is off at the next start.
- When the game runs slower than a DS (a slow device), the sound is stretched to fill the
  time, at its own pitch, instead of breaking up into crackles. At full speed it plays
  exactly as before.
- Fixed: a zoomed or rotated background could drift and smear into streaks when parts
  of it stayed the same from one frame to the next.
- The setup program for macOS can now check for updates and update itself from
  the setup window and with `--update`, matching Linux and Windows.
- A single-screen build of the decompilation (the one-screen game) is recognised when it
  starts and shown as the screen the game itself has chosen, with no merging or
  widescreen changes. The choice is not saved in the settings.

## v0.4.2 — Faster widescreen

**Downloads:** the same setup programs as before; update from the setup window
and run the setup again to rebuild the game (your save is kept). Android
players: press **Build for Android** again and install the new app over the old
one.

- **The widescreen 3D picture takes about half the processor time it did.** It is
  drawn several pixels at a time (SIMD: AVX2 or SSE on PCs, NEON on phones), and
  its working memory now stays in the processor's cache. On PCs it also uses fewer
  threads (about one per core), which leaves more room for the game itself. The
  picture is the same, apart from the odd pixel along the edge of a polygon.
- The widescreen battle view looks for the stats window in one pass instead of four.

## v0.4.1 — Android controllers and update notice

**Downloads:** the same setup programs as before; update from the setup window
and run the setup again to rebuild the game (your save is kept). Android
players: press **Build for Android** again and install the new app over the old
one.

- **Android: controllers such as the Backbone One work.** SDL's own USB drivers
  took over some pads (the Backbone One, PlayStation pads) from Android's
  input, and lost them when they could not open them themselves. All controllers
  now come through Android, which knows them.
- **Android: the app says when there is a new version.** At start it asks
  GitHub for the newest release; when that is newer, a message at the top of the
  screen says to build the app again on the PC (the app holds your own game, so
  it cannot download itself). It is the only thing the app uses the internet for.
- **Lower world detail for slower devices.** WORLD DETAIL in the pause menu now
  also offers 384, 288 and 192 (the DS's own sharpness) as well as 540 to 1440;
  fewer lines draw faster. Left and right go down and up through the choices.
- **FPS counter.** FPS COUNTER in the pause menu shows the frames shown per
  second in the top right corner.

## v0.4.0 — Faster again, cleaner menus

**Downloads:** the setup programs for Linux (x86_64 and arm64), Windows and now
macOS (Apple Silicon and Intel; the Mac app is unsigned, so open it with
right-click → Open the first time). Use the setup window's update button or
download the new one, then run the setup again to rebuild the game (your save
is kept).

- **The bag shows just the bag.** The item description box from the other
  screen is no longer drawn below it.
- **A tidier Pokémon summary.** The Pokémon's name, item and markings sit in
  their own column and the information from both screens is shown side by side, each
  in a rounded card with its contents centred, and no borders cut off.
- **Menus stay on one layout.** Opening the bag, party, Pokégear and the other
  menus from the start menu no longer shows the top screen and then both screens
  before the menu's own view, and closing them no longer falls back to the plain
  bottom screen while they fade out. The view now follows each menu from the
  moment the game starts loading it until it has closed (the game's start and
  end functions of each menu are watched as well as its main one).
- **Faster again: about 30% in the overworld and 20% in battles** (measured
  headless on a 6-core desktop; slower machines such as the Steam Deck and
  phones gain the most). The bottom screen and anything that does not move are
  no longer redrawn every frame, the CPUs switch less often, waits for the game
  card and the touch screen are skipped, and more instructions are recompiled.
  Nothing else changes: in testing, pictures, sound and memory came out the
  same, apart from a one-line difference in a few frames of battle transitions.
- `run.sh` writes `game.log` next to it, as the setup window's Play button does.
- The log lists every controller SDL sees (name, USB IDs, whether its layout is
  known), the SDL version, and the Steam and container settings that can hide a
  controller, for reports like #8.

## v0.3.0 — Faster, updates, logs

**Downloads:** the same three setup programs; use these instead of the earlier
ones and run the setup again to rebuild the game (your save is kept). From this
version on, the setup window can update itself.

### Faster

- **The recompiled game code is about 6–9% faster than melonDS's interpreter**
  (before it was about 16% slower). The common instructions are now plain C++
  instead of calls into the interpreter, and memory accesses and jumps take a
  shorter path. Every instruction was checked against the interpreter
  (`--lockstep`) over 16,000 frames of booting, walking and battling.

### Fixed

- **White strips beside the screen as a battle opens.** The widescreen view showed
  the half of the opening's background, and the 3D pieces, that the game keeps off
  the screen; a row the screen shows in one colour is now that colour beside it.
- **The trainer's legs over the black bar** as he slides out after throwing the
  Poké Ball: beside the screen he is now behind the message box, as on the screen.
- **Fades at the start and end of a battle** reached the sides of the view a frame
  before the screen, so the picture faded unevenly. The sides now use the palettes
  and brightness the screen was drawn with.
- **The stats after a level up** were cut in two, the top half moved to the right
  with the wild Pokémon's side. Windows like it now stay where the game puts them.

### Added

- **Logs for bug reports.** Started from the setup window, the game's messages now
  go to `game.log` (new each time) and a build's output to `build.log`; **Open logs
  folder** opens the folder with both. On Android `log.txt` moved to
  `Android/data/io.github.clickingmaster.ssrecomp/files`, where a file manager or a
  PC can reach it. The renderer in use is now always in the log.

- **Updates from the setup window.** At start it checks for a newer release (shown
  next to the version number); **Update** downloads the setup program for your
  system, checks it against the release's `SHA256SUMS.txt`, puts it in place of
  the old one and restarts. Then press **Build** once to update the game; the
  save and settings are kept. (This version still has to be downloaded by hand.)
- **Issue forms** on GitHub ask for the version, system and log.

- **Portable mode** (#10): with a file `portable.txt` next to the setup program,
  everything is kept in a folder `ssrecomp-data` beside it instead of
  `%LOCALAPPDATA%` (Windows) or `~/.local/share` (Linux). The ROM's location is
  remembered relative to that folder, so it still works on another PC or drive
  letter.

## v0.2.6 — Controller fixes (prerelease)

**Downloads:** the same three setup programs; use these instead of the earlier
ones and run the setup again to rebuild the game.

### Fixed

- **Controllers not working** (#8). Only the first controller found was used;
  on Windows that is often a virtual or idle device (from Steam, DS4Windows,
  vJoy, a racing wheel...), and then nothing pressed on any controller worked.
  Every connected controller now works at once.
- **Generic USB pads** that SDL does not know the layout of were ignored. They
  are now read with the usual layout: the first buttons as A, B, X, Y, then L,
  R, Select, Start, with the D-pad and the left stick. A device whose stick
  rests at one end (some keyboards and mice report themselves as joysticks) has
  its stick ignored, so the player does not walk on their own.
- The controller now also works while the game window is not focused, as when
  it was started from the setup window.

### Tested

- On Linux with a real Xbox Series controller and virtual ones (an Xbox 360
  pad, a generic USB pad, one with a stuck stick). Compiled for Windows, not yet
  tried with a controller there.

## v0.2.5 — Widescreen battles

**Downloads:** the same three setup programs; use these instead of the earlier
ones and run the setup again to rebuild the game (and the APK, on Android).

### New

- **Battles fill the wide screen** instead of having stretched sides. The
  battle scene is drawn wider than the DS screen: the background and the
  platforms continue past its edges, the HP boxes move out to the sides of the
  view, and the wild Pokémon slides over to the right as your own Pokémon is on
  the left. The trainer throwing the Poké Ball walks all the way off screen.
- Move effects stay with the Pokémon they belong to, and the message box is
  left where it is.

### Fixed

- **The battle strip vanished for a moment** between choosing FIGHT and the
  moves appearing, showing the message box under it.

### Known limitations

- Tried on 16:9 only; wider phone screens (20:9) are untested in battles.
- A move effect crossing the gap between the two Pokémon can look stretched
  there for a moment.

## v0.2.4 — Colour fixes

### Fixed

- **Washed-out colours on Windows** (#5): the game asked SDL for its OpenGL ES
  renderer, which on Windows (seen with AMD graphics) can convert every pixel
  as if it were linear light, so the whole picture looked pale. Windows now uses
  SDL's default, Direct3D.
- **The game was always at night.** The DS clock started at midnight on
  1 January 2000 on every launch, so the overworld always had night lighting.
  It now follows the computer's clock, as on a real DS, including after loading
  a quick save.

## v0.2.3 — Android (experimental)

**Downloads:** the same three setup programs; use these instead of the earlier
ones. There is no APK to download: the Android version contains the game's code,
so it is built from your own ROM on your PC, like the PC version.

### New

- **Build for Android** in the setup program (`--android` without a window).
  It downloads Google's Android NDK and SDL2 (once, checked against their
  published checksums), compiles the game for 64-bit ARM Android 7.0+, and makes
  a signed APK to copy to the phone or handheld and install. No Java or Android
  Studio is needed: the APK is packed and signed by `tools/android/apk.py`.
- **On Android:** full screen in landscape; on-screen buttons (D-pad, A/B/X/Y,
  L/R, Start, Select, Menu), hidden while a controller is being used and back at
  a touch; fingers on the picture touch the game's touch screen, several at
  once; Back opens the pause menu; the save is written when the app goes to the
  background; messages and frame timings go to `log.txt` in the app's storage.
- **Tested in Google's Android emulator** (Android 14): installs, updates over
  itself, loads a save, plays with the on-screen buttons and touch, pause menu.
  **Not yet tried on a real phone or handheld**: reports welcome, with
  `log.txt` if something is wrong.

## v0.2.2 — Windows fixes

**Downloads:** the same three setup programs as before; use these instead of the
earlier ones. Already built? Start the new setup program and press **Build**
once: only the changed files are recompiled and your save is kept.

### Fixed

- **Windows: progress was not saved** (#2). Only the first save ever reached
  `soulsilver.sav`; later ones were left in `soulsilver.sav.tmp`, because
  Windows does not let a file be renamed over an existing one. Saves now replace
  the file. If you lost progress this way, see the note below.
- **Windows: the build stopped with "junk at end of line"** in `rc_images.S`
  (#3). The generated assembly ended with a Linux-only line.
- **Windows: "Could not find a package configuration file provided by SDL2"**
  (#3). The setup program now uses MSYS2's CMake ahead of any other copy on the
  PATH, tells CMake where MSYS2's libraries are, and says when SDL2, libpng or
  zlib are missing.
- **Screen jumping around while saving** (#1). During "Saving a lot of
  data…" the layout fell back to the X menu's (the save card beside the
  overworld) and the picture flipped between widescreen and 4:3, because the
  game hardly runs anything else while it writes. The save screen now stays as
  it is until the save is written.
- **Clearer install instructions** in the setup program (#4): it says to open a
  terminal, has a button to copy the commands, and on SteamOS (Steam Deck)
  gives the container steps, as there programs cannot be installed directly.

### Getting back lost progress on Windows (#2)

Close the game, open `%LOCALAPPDATA%\ssrecomp`, rename `soulsilver.sav` to
`soulsilver.sav.old`, then rename `soulsilver.sav.tmp` to `soulsilver.sav`. The
`.tmp` file holds your latest save.

### Tested

- The whole game cross-compiled for Windows with MinGW and run under Wine:
  identical to the Linux build frame for frame, and saving in the game replaces
  the save file. Still not tried on a real Windows PC.

## v0.2.1 — 64-bit ARM Linux

**Downloads:** `ssrecomp-setup-linux-x86_64`,
`ssrecomp-setup-linux-aarch64.AppImage` (64-bit ARM Linux: Raspberry Pi 4/5
with a 64-bit OS, other arm64 boards and laptops) and
`ssrecomp-setup-windows-x86_64.exe` are setup programs, not the game: start
one, choose your own Pokémon SoulSilver (US) ROM, press Build, then Play. Use
these instead of the v0.2.0 ones.

### New

- **Setup program for 64-bit ARM Linux**, as an AppImage
  (`tools/make_appimage.sh`). The game compiles for arm64; cross-compiled and
  run under emulation it is identical to the x86 build frame for frame. Not yet
  tried on real ARM hardware.

### Fixed

- **The game would not compile on ARM, and would crash on older x86 CPUs**: the
  widescreen renderer was always built with AVX2 instructions. They are now
  used only on x86 CPUs that have them; nothing changes on those.
- The recompiler failed when its output folder was on another drive.

### Known limitations

- Windows and ARM builds not yet tried on real hardware (Windows under Wine,
  ARM under emulation).
- See v0.2.0 and v0.1.0 for the rest.

## v0.2.0 — build from your own ROM

**Downloads:** `ssrecomp-setup-linux-x86_64` and
`ssrecomp-setup-windows-x86_64.exe` are setup programs, not the game: start
one, choose your own Pokémon SoulSilver (US) ROM, press Build, then Play. A
compiler and a few libraries are needed; the program says which and how to get
them. A ready-built game cannot be offered, as it would contain the game's code.

### New

- **Setup programs for Linux and Windows**: one file each, made from this
  release's source (`tools/package_setup.py`). They carry the project's source
  and build the game in a folder of their own.
- **Setup window** (`ssrecomp_setup.py`): pick your ROM, press Build, press
  Play. It checks the ROM is the right game, says what is missing to compile
  and how to install it, picks the number of compile jobs from your memory, and
  shows the progress. `python3 ssrecomp_setup.py --rom <file>` does the same
  without a window.
- **No decompilation needed to play.** The code is recompiled straight from
  your ROM (`tools/ndsrom.py` reads and decompresses it); what the
  decompilation used to add comes from `layouts/soulsilver-us.json`, which holds
  addresses and checksums only. The result is identical to building from the
  decompilation. `setup.sh` remains for working on the game's source.
- **Windows (untested):** the code compiles with MinGW, and the setup window
  supports MSYS2 and copies the needed DLLs next to the game. Not yet tried on
  a Windows machine.

### Known limitations

- Tested on Linux only (Ubuntu 24.04 in a container on Bazzite).
- Only Pokémon SoulSilver (US).
- See v0.1.0 for the rest.

## v0.1.0 — first preview

The first public version. Playable from the start of a new game through the
early routes, towns and gyms; the rest of the game has not been played through
with it yet.

**No ready-made download.** A built program would contain the game's code, so
releases are source only: run `./setup.sh` with your own copy of Pokémon
SoulSilver (US) as described in the README.

### What's in it

- **Static recompilation** of the game's ARM9 code from pret/pokeheartgold's
  build output, running on the melonDS core. Checked against melonDS's
  interpreter instruction by instruction (`--lockstep`).
- **One screen, no switching.** Each situation has its own layout that takes the
  useful parts of both DS screens: the overworld with dialogue, the X menu beside
  the overworld, the bag, party, Pokédex, Pokégear, PC, shops, the intro and the
  starter choice.
- **Widescreen overworld:** the 3D world is drawn across the whole 16:9 view at
  a higher resolution (540–1440 lines, "WORLD DETAIL" in the pause menu); trees
  and buildings at the sides are drawn instead of popping in.
- **Battles across the whole screen**, with the touch-screen buttons as a strip
  along the bottom: FIGHT / BAG / POKéMON / RUN and the four moves, chosen with
  left and right, A to pick, B to go back.
- **60 fps:** in-between frames for the 3D scene, which the game updates 30
  times a second.
- **Controller first:** everything is reachable with buttons; a pause menu (Guide
  button or Esc) holds the settings, save states and fast forward.
- **Runs well:** about 9 ms of a 16.7 ms frame at 2560×1440 on a 6-core desktop
  CPU; the picture is composed on the graphics card.
- **`setup.sh`** installs what is missing, builds the game with the
  decompilation, checks it against the retail ROM's SHA-1 and compiles
  everything in one step.

### Known limitations

- Linux only so far (tested on Ubuntu 24.04 in a container on Bazzite).
  Windows, macOS and Android builds do not exist yet.
- Only Pokémon SoulSilver (US). HeartGold and other regions are not supported.
- Later parts of the game, and some screens (radio, contests, the Pokéathlon,
  wireless features), have not been checked and may show a generic two-screen
  layout.
- The widescreen picture has no edge-marking outlines; battles, menus and the
  intro stay 4:3 apart from the battle sides.
- The recompiled code is not yet faster than melonDS's own interpreter; making
  it so is future work.
