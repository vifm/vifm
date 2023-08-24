# saf-revisions Vifm plugin

Install vifm plugin by copying or symlinking saf-revisions (plugin folder) to $VIFM/plugins/ (usually ~/.config/vifm/plugins/ or ~/.vifm/plugins/). This will make :Saf* commands available in Vifm:

```
:SafRevisionsToggle     -- Activate or deactivate saf-revisions
:SafRevisionsActivate   -- Turn saf-revisions on
:SafRevisionsDeactivate -- Turn saf-revisions off
:SafPreviousRevision    -- Starfield! Warp! Magic! Back in time.
:SafNextRevision        -- Starfield! Warp! Magic! Forward in time.
```

Map appropriate keys to saf-revisions plugin functions, pick any unused keys that work for you:
``` bash
nnoremap @ :SafRevisionsToggle<cr>
nnoremap < :SafPreviousRevision<cr>
nnoremap > :SafNextRevision<cr>
```
Start Vifm, then in any folder backed up with saf to local file system backup target (reaching remote targets is not possible at the moment) execute `:SafRevisionsToggle` command, or with definitions like above press `@`. Navigate trough all the backups where folder content is changed with `:SafPreviousRevision` and `:SafNextRevision` functions or keys mapped to those functions. Switch back to normal with another `:SafRevisionsToggle`.

> **_NOTE:_**  Please keep in mind that you will be browsing actual backup folder content. Changing such content is possible but usually a bad idea. Limit interaction to review and copy content to backup source folder, not the other way around.
