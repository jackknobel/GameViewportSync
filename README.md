# Game Viewport Sync
UE4 Plugin for syncing the active Play In Editor session with the other Level Editor Viewports.

[![Preview](https://i.imgur.com/5PEwR0p.gif)](https://gfycat.com/responsiblemealyheifer.gif)

**Supported Unreal Versions:**

4.24 - 4.25.

**Features:**
- Simple Setup
- Actor tracking/following with orbit camera controls 
  - If the follow actor does not exist at level start up it will be automatically attached when it becomes available
- Per viewport setting toggle

**Getting Started:**
- Add the plugin to your project ([GameDirectory]/Plugins/) folder (either via cloning or from the releases tab)
- Open the editor.
- That's it! (You can add additional level editor viewports to the drop down the usual way via Window->Viewports menu)

**Configuration:**

All settings can be configured via the viewport window drop down menu:

[![DropDownConfig](https://i.imgur.com/24HuxOol.gif)](https://i.imgur.com/24HuxOo.gif)

You can follow an actor via the menu above, right clicking on the actor itself (see gif below) or via the shortcut ALT + SHIFT + F)

[![RightClickContextMenuConfig](https://i.imgur.com/eKs9jPFl.gif)](https://i.imgur.com/eKs9jPF.gif)


*Note:*

Currently does not support persistent viewport settings. 

Follow the issue here:

https://github.com/jackknobel/GameViewportSync/issues/1
