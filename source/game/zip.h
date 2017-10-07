#pragma once

/*
Thanks for information about zip format

http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html (Japanese)
https://hgotoh.jp/wiki/doku.php/documents/other/other-017 (Japanese)
https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT (English)

*/

u32 ZipExtractContent(const char* path, const char* extrpath, u32 CO, u32* newCO, char* ErrorDesc, u32* flags);
bool ZipExtract(const char* path, const char* extrpath, u32* flags);