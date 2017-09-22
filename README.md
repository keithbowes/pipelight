# Pipelight

Pipelight is a special browser plugin which allows one to use Windows-only plugins inside NPAPI plugin-compatible \*nix browsers.  We are currently focusing on Silverlight, Flash, Shockwave and the Unity Webplayer.  The project needs a patched version of WINE to execute the plugins. 

## Requirements

### Platform
Pipelight requires a \*nix operating system (such as Linux or FreeBSD) running on an x86 (e.g. AMD or Intel) processor.  Most non-mobile computers use an x86 processor.

### Browser
Pipelight Requires a browser that supports NPAPI plugins.

#### Gecko
##### Firefox
Firefox versions under 52 work (as does the Firefox 52 ESR).  After that, [NPAPI plugins are disabled](https://blog.mozilla.org/futurereleases/2015/10/08/npapi-plugins-in-firefox/) for everything but Flash.

#### SeaMonkey
[SeaMonkey](https://www.seamonkey-project.org/) versions under 2.50 support NPAPI plugins.  SeaMonkey 2.50 nightlies still support both Flash and Silverlight plugins.

#### Palemoon
[Palemoon](http://www.palemoon.org/) pledges to support NPAPI plugins indefinitely.

### WebKit
Webkit still supports NPAPI plugins, so you can use browsers such as [Midori](http://midori-browser.org/) and [Uzbl](https://www.uzbl.org/) without a problem.  However, these browsers may need to run the plugins in external windows (put <samp>embed = no</samp> in your configuration file or set the <var>PIPELIGHT_EMBED</var> environment variable to 0).

### Blink
Chrome/Chromium versions under 34 work.  You'll have to apply [a couple patches](https://bugs.launchpad.net/pipelight/+bug/1307989) to get Chromium versions 34 or higher to work.  Similarly, other browsers based on Chromium 34+, like Opera 15+ and Vivaldi, won't work.

### Presto
Older versions of Opera use Presto rather than Blink, so you can use Opera versions under 15 without a problem.

## Installation
To install, you must [compile WINE with the WINE Staging patches](http://web.archive.org/web/20160815170857/http://pipelight.net:80/cms/page-wine.html), [compile Pipelight](http://web.archive.org/web/20160815170857/http://pipelight.net:80/cms/install/compile-pipelight.html), and [enable the required plugins](http://web.archive.org/web/20160815170857/http://pipelight.net:80/cms/installation.html#section_2).

See also [Gentoo's Netflix/Pipelight Wiki page](https://wiki.gentoo.org/wiki/Netflix/Pipelight) for some tips and troubleshooting advice.

### Pre-compiled binaries
Unfortunately, Michael Müller, the initiator of the project, has given up on the project and has removed the repositories for supported distributions.  One must now compile from source, which isn't difficult.

### Browser spoofing
Unfortunately, services like Netflix detect one's browser instead of the browser's capabilities, so you may need to install a [user-agent switcher](https://github.com/keithbowes/user-agent-switcher).

#### Uzbl
If you use Uzbl and have the per-site settings script loaded, you can add this to <var>$XDG\_DATA\_HOME</var>/uzbl/per-site-settings:
<pre>.netflix.com
    .*
        set useragent Mozilla/5.0 (Windows NT 5.1; rv:31.0) Gecko/20100101 Firefox/31.0 @(+sh -c "pipelight-plugin --version | sed -e 's/\\s\\+/\\//g'")@</pre>

#### Standalone programs

If your interest is merely to watch Netflix (the original purpose of Pipelight), you can try an app like [Netflix Penguin](https://github.com/ergoithz/netflix-penguin), so that you won't have to install such an extension in your browser.

## Alternatives
Microsoft has deprecated Silverlight and now streaming services are increasingly using HTML5 with EME.  For such streaming services to work, you must use a browser that supports EME with the proper DRM plugin (usually Widevine).  Such browsers include [Google Chrome](https://www.google.com/chrome/index.html) (37 or higher) and official builds of [Firefox](https://mozilla.com/) (52 or higher).  Unfortunately, Chromium and other browsers based on it (Vivaldi, Opera, etc.) don't support EME, as they lack Google's proprietary DRM code.  Similarly, only the prebuilt versions of Firefox from the Mozilla website support EME, but those built by yourself or by your distribution don't.
