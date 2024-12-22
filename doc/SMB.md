The first task was to get an understanding of the game as a player and also
technically. The first part, there are tutorials. The second part, there is the
disassembly. Read / Skim it and mess with it. Create TASes watch memory
addresses, tinker etc.

A very early task was to extract all of the nametables from all of the
levels. That process is not currently included in this repository. At a high
level I did the following:

 1) Identify the relevant memory addresses, in this case by reading the
    excellent disassembly from doppelganger et al.

    `AREA_DATA_LOW`           = 0x00e7,
    `AREA_DATA_HIGH`          = 0x00e8,
    `SCREENEDGE_PAGELOC`      = 0x071a,
    `SCREENEDGE_X_POS`        = 0x071c,

 2) Construct a series of TASes that go through all of the areas
 3) On the appropriate frame extract the nametable and store it in the
    database

A few failed attempts before this would extract images (pngs) of the
backgrounds and stitch things together semi-manually and all that. But then
how to handle nametable changes? Like coins being collected or blocks
breaking? Using the nametable data directly was the key.

The inputs and relevant frames identified in this process are stored in
`data/smb/nt_extract_tas.sql` and `data/smb/nt_extract_record.sql`

Then I wanted a minimap. This a 4 bit image of the entire world. The tool for
this is in another repo. But the data is in data/smb/minimap.sql

