NOTE: Source code will be released soon! I just want to finish a little documentation for programmers.

# What is this?

This is a program which should be able to extract ALL possible .dta files, which are used by the game "Hidden & Dangerous 2" and also its demos.
These .dta files in the games directory are archives, which include multiple files like textures, models, scripts etc. They are encrypted and most of them are compressed.
This is not the first program which is able to extract these files but what I have seen in the past was not very user friendly in my opinion.
The HD2unpacker uses GUI elements (windows, buttons etc.) from the windows API, so it is just a matter of a few clicks to extract the requested content.
You also do not need any additional .dll files. It can run in any location you want and you can specify where you want to save the extracted content.
It also does not matter what the file names are. The unpacker can identify the archives by their structure in order to apply the correct decryption key.
For example, if you use "Maps_backup.dta" or you called your backup "Textures.dta" (based on Maps.dta) the unpacker should still be able to work with it.


# What does it include?

Currently it can extract 15 different .dta archives. This is all I was able to find yet. Please contact me if something is missing or if you can not extract an archive.
It is my goal that this can unpack everything. The unpacker may not work if you have modified the .dta file data.
Also do not try to unpack files from the game "Mafia" with it. This won't work. Although I could apply the exact same methodes, I don't own Mafia, therefore I could not check its file structures.

Anyway, the 15 .dta archives mentioned earlier are the following:
- Maps.dta
- Maps_C.dta
- Maps_U.dta
- Missions.dta
- Models.dta
- Others.dta
- Sounds.dta
- Scripts.dta
- SabreSquadron.dta
- Patch.dta
- PatchX01.dta
- LangEnglish.dta
- demo.dta (H&D2 Singleplayer Demo)
- demo.dta (H&D2 Multiplayer Demo)
- demo.dta (H&D2 Sabre Squadron Demo)

If you know more (related to H&D2), let me know.


# Usage

It may not look very pretty but it does its job.
You just start the HD2unpacker.exe from any locations you want.
Hit the top button "SELECT FILE" and, well, select the archive you want to extract, e.g. "Maps.dta".
You find these .dta files in your main directory, where Hidden & Dangerous 2 is installed.
Now, after you have confirmed it you should see in the status (above the button) the archive you have loaded.
At this point it was sucessfully mounted and is now ready to extract everything.
If you press "SET DESTIONATION AND EXTRACT" a new window will open, where it asks you about the location you want to extract to. You can select any folder you like. In the same window you can also create a new folder and give it a name.
As soon as you hit "OK" it will start the extraction process.

You should see a progress bar, which gives you an idea of how long the extraction take.
Above the progress bar you see the file it is currently extracting.
Below the progress bar is a cancel button, which does what you expect. It will cancel the extraction and tell you how many files it has already extracted. It will not delete the already extracted files, keep that in mind.
Also keep in mind that the program will overwrite files, which have the same names already, without asking you to do so.
This might be important if you extract it in the same folder, where you have modified files you want to keep already.


# Credits

I had to read and learn alot about these archive structures to do this.
I want to thank culticaxe for his help and testing.
I thank FlashX to point me into the right direction by providing me some really helpful information.
A huge help was the following GitHub page, where Mafia files are processed:
https://github.com/MafiaHub/OpenMF-Archived
I have taken some of their functions and modified them to my needs. Some by quiet alot, to make the program more efficient and useful for my needs.
I want to thank HDmaster for his research about the LZSS decompression these files are using. I used his function but changed it to a more C-Style way.

Furthermore I used the following page as reference, the decryption function shown here is identical to what HD2unpacker uses:
https://hd2.fandom.com/wiki/DTA
