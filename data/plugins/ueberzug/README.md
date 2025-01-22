## Ueberzug

Plugin to integrate a Überzug-compatible graphical previewer.  Known
implementations:

| What                    | Where to find
| :--:                    | :-----------:
| Original (discontinued) | https://github.com/seebye/ueberzug
| Fork of Python version  | https://github.com/ueber-devel/ueberzug
| Improved rewrite in C++ | https://github.com/jstkdng/ueberzugpp

### Installation

Copy this directory to `$VIFM/plugins`.  `$VIFM` is likely to be either
`~/.config/vifm` or `~/.vifm`.  Create the plugins directory if it doesn't
exist.

### Configuration

Copy lines corresponding to filetypes that you wish to preview to your `vifmrc`
(usually to be found at `$VIFM/vifmrc`).

```vim
" If you are using ueberzugpp, you can have animated gif previews with this:
fileviewer <image/gif>
    \ #ueberzug#image_no_cache %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear

" Otherwise, use the video fileviewer for gifs
" fileviewer <video/*>,<image/gif>
fileviewer <video/*>
    \ #ueberzug#video %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear

fileviewer <image/*>
    \ #ueberzug#image %px %py %pw %ph
    " or \ #ueberzug#image_no_cache %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear
fileviewer <audio/*>
    \ #ueberzug#audio %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear
fileviewer <font/*>
    \ #ueberzug#djvu %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear

fileviewer *.pdf
    \ #ueberzug#pdf %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear
fileviewer *.epub
    \ #ueberzug#epub %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear
fileviewer *.djvu
    \ #ueberzug#djvu %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear

fileviewer *.cbz
    \ #ueberzug#cbz %px %py %pw %ph
    \ %pc
    \ #ueberzug#clear
fileviewer *.cbt
    \ #ueberzug#cbt %px %py %pw %ph %c
    \ %pc
    \ #ueberzug#clear
fileviewer *.cbr
    \ #ueberzug#cbr %px %py %pw %ph %c
    \ %pc
    \ #ueberzug#clear
fileviewer *.cb7
    \ #ueberzug#cb7 %px %py %pw %ph %c
    \ %pc
    \ #ueberzug#clear
```

### Optional dependencies

They used for viewing corresponding file types.

 * [ImageMagick](https://imagemagick.org/) (non-optional for `#image` handler,
    use `#image_no_cache` if this dependency isn't installed or caching isn't
    needed)
 * [ffmpegthumbnailer](https://github.com/dirkvdb/ffmpegthumbnailer)
 * [fontpreview](https://github.com/sdushantha/fontpreview)
 * `pdftoppm` (can be installed with [poppler](https://poppler.freedesktop.org))
 * [ddjvu](http://djvu.sourceforge.net/doc/man/ddjvu.html)
 * [gnome-epub-thumbnailer](https://gitlab.gnome.org/GNOME/gnome-epub-thumbnailer)
 * [fuse-zip](https://bitbucket.org/agalanin/fuse-zip)
 * [rar2fs](https://github.com/hasse69/rar2fs)
 * [fuse3-p7zip](https://github.com/andrew-grechkin/fuse3-p7zip)
 * [archivemount](https://github.com/cybernoid/archivemount)

### Credits
 * Many of the thumbnailer commands are copy pasted from
   [vifmimg](https://github.com/thimc/vifmimg).

### Authors
 * [xaizek](https://github.com/xaizek/)
 * Mahor Foruzesh ([mahor1221](https://github.com/mahor1221))
