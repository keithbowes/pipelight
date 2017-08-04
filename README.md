# Pipelight

Pipelight is a suite of tools that allows you to run various Windows plugins under Linux via WINE.  Its main objective is to allow you to watch streaming videos on services using Microsoft's Silverlight, but some other plugins also work.

## Installation
To install, you must [compile WINE with the WINE staging patches](http://web.archive.org/web/20150529164221/http://pipelight.net:80/cms/page-wine.html), [compile Pipelight](http://web.archive.org/web/20150524055939/http://pipelight.net:80/cms/install/compile-pipelight.html), and [enable the required plugins](http://web.archive.org/web/20150524054351/http://pipelight.net:80/cms/installation.html#section_2).

### Pre-compiled binaries
Unfortunately, Michael Müller, the initiator of the project, has given up on the project and has removed the repositories for supported distributions.  One must now compile from source, which isn't difficult.

### Browser spoofing
Unfortunately, services like Netflix detect one's browsers instead of whether it supports EME with Widivene or has the Silverlight plugin installed, so you may need to install a [user-agent switcher](https://github.com/keithbowes/user-agent-switcher).

## Alternatives
Microsoft has deprecated Silverlight and now streaming services are increasingly using HTML5 with EME.  For this to work, you must use a browser that supports EME with the proper DRM plugin (usually Widivine).  Such browsers include [Google Chrome](https://www.google.com/chrome/index.html) and official builds of [Mozilla Firefox](https://mozilla.com/).  Unfortunately, other browsers based on Chromium (Vivaldi, Opera, Chromium itself, etc.) won't work, as they lack Google's proprietary DRM code.  It's the same with Firefox; only the prebuilt versions from the Mozilla website will function, but those built by yourself or by your distribution won't.  Note that starting in Chromium 34, Pipelight won't work either, at least not without applying a patch to [restore support for NPAPI plugins](http://web.archive.org/web/20150515083611/http://pipelight.net:80/cms/chrome-chromium.html).

Support for NPAPI plugins will be removed from Firefox after version 52, but [some other Gecko-based browsers](https://seamonkey-project.org/) will continue to support them to at least some degree.  Webkit-based browsers (such as [Midori](http://midori-browser.org/) and [Uzbl](https://www.uzbl.org/)) also work fine.
