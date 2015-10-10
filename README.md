Enterprise
==========
[![Build Status](https://travis-ci.org/SevenBits/Enterprise.png)](https://travis-ci.org/SevenBits/Enterprise)
[![Bitdeli Badge](https://d2weczhvl823v0.cloudfront.net/SevenBits/enterprise/trend.png)](https://bitdeli.com/free "Bitdeli Badge")

This source code archive contains code for Enterprise, a custom UEFI boot manager designed to load Linux distributions, even those without UEFI booting support, directly from ISO files on UEFI-based computers. Enterprise was originally designed to compliment [Mac Linux USB Loader](https://github.com/SevenBits/Mac-Linux-USB-Loader), but it can also be used on its own to boot Linux on a variety of UEFI-based PCs and Macs. Enterprise provides an easy-to-use and simplistic interface to GRUB, automating many of the tasks necessary to boot distributions of Linux from an ISO file.

Enterprise supports booting multiple distributions, so you can have more than one distribution per USB stick and multiple configurations for each distribution.

Enterprise requires a configuration file telling it about which distributions it should load. This configuration file is created automatically when you use tools like [Mac Linux USB Loader](https://github.com/SevenBits/Mac-Linux-USB-Loader), though it is possible to write your own file and configure Enterprise as one would configure other boot managers such as GRUB, gummiboot, and syslinux, albeit much more simply.

Now, with that out the way (phew!), let's get to the interesting stuff, shall we?

### LICENSE ###

Enterprise is under the LGPL. Enterprise is not *only* my program; it pulls in code from other software projects (namely, gummiboot), and I want to make sure that not only they get credit from their work, but that others can benefit from what I've done as well. Releasing Enterprise under the LGPL allows me to most easily do this.

### CODING STYLE ###

I've decided to establish, up front, a coding style for Enterprise for anyone that wants to contribute. Most important: braces and if-statements. I'll sum up my policy in one code block:

    if (true)
        bob();
    else
        sam();

is _not_ complaint. This is how I prefer it:

    if (true) {
        bob();
    } else {
        sam();
    }

For those of you in the know, I code using a style called [1TBS](https://en.wikipedia.org/wiki/Indent_style#Variant:_1TBS "One True Brace Style") - or, the "one true brace style". I quite like it. So much so, in fact, that it's the required syntax style (just so things don't get messy).

Same goes with functions, other blocks like `while`, `do`, and pretty much anything else. Please realize that not everything will be under this style (particularly code I've pulled in from elsewhere), but I'll do my best to follow this style, and if you contribute, I expect you to do yours.

Secondly, all source code files must use *tabs* - not spaces, and I expect a tab width of 4. If your editor is configured to use spaces, you *must* convert the spaces into tabs with a width of 4. This can be done with a UNIX command like `unexpand --tabs=4`.

### PULL REQUESTS ###

I will accept pull requests on two conditions:

1. Your code follows my coding styles (mentioned above) and
2. Your code is licensed under an LGPL-compatible license and is readable and decently commented.

Assuming that your code is in compliance with these rules, I usually will accept pull requests that either: add a feature/fix a bug that is listed above or fix an issue filed through GitHub.

### FINAL THOUGHTS ###

That's about it, I think. Hope this helps someone.
