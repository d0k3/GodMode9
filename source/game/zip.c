#include "fsutil.h"
#include "ui.h"
#include "vff.h"
#include "fsperm.h"

const u32 ZIPLFH = 0x04034B50;

/*
Thanks for information about zip format

http://www.tvg.ne.jp/menyukko/cauldron/dtzipformat.html (Japanese)
https://hgotoh.jp/wiki/doku.php/documents/other/other-017 (Japanese)
https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT (English)

*/

u32 ZipExtractContent(const char* path, const char* extrpath, u32 CO, u32* newCO, char* ErrorDesc, u32* flags){
    /* return is error code
    0 : successed (or skipped)
    1x : File i/o error and not continuable
        10 : Failed to get compressed size
        11 : Failed to get original size
        12 : Failed to get File name length
        13 : Failed to get Comment length
        14 : Failed to get File name
        15 : Failed to get zip version to extract
        16 : Create new file to extract to failed
        17 : Inject (Actual file data) failed
        18 : The archive does not have enough size to inject data to another file
	20: User abort on asking action about path already exists
    3 : Archive maybe compressed (decided by checking file size of compressed and original one)
    4 : File name length is more than 256
    6 : Archive is compressed or encrypted (decided by the version to extract is 2.0 and content name ends with '/')
    7 : The content is encrypted, compressed or used ZIP64 (decided by checking the version to extract)
    */
    
    
    bool silent = (flags && (*flags & SILENT));
    u32 ArchiveSize = FileGetSize(path);
    u32 FileNameLength = 0;  // File name length
    u32 FileSize = 0;        // FileSize (compressed one)
    u32 FilSize_org = 0;     // FileSize (original one)
    u16 FileCommentSize = 0; // Length of comment field
    u8  ErrorCode= 0;        // temporary error code  (used when error number 3 or 4 occurred)
    u32 FileDataStart_o;     // start offset of adtually data
    u32 FileDataEnd_o;       // end offset of adtually data
    u8  ZipVersion = 0;      // zip version to extract
    u8  ContentType = 0;     // 0 : non-compressed file, 1 = folder
    memset(ErrorDesc, '\0' , strlen(ErrorDesc));
	
    // these 5 variables are some path
    char FileName[256] = "";     // content name (e.g. File : "testdir/testfile.txt" Dir : "testdir/testsubdir") (last slash will be removed in the process)
    char FileinDir[256] = "";    // the path of dir of content (e.g. File : "testdir" Dir : "testdir")
    char RealFileName[256] = ""; // the content real name (e.g. File : "testfile.txt" Dir : "testsubdir")
    char File_extr_f[256] = "";  //  the full path of the file (e.g. File : "0:/extract/testdir/testfile.txt" Dir : "0:/extract/testdir/testsubdir" (but not used))
    char extrpath_l[256] = "";   // the full path of dir of content or full path of the folder(e.g. File : "0:/extract/testdir" Dir : "0:/extract/testdir/testsubdir")
    
    // Get size of compressed and original one to check compression
    // pick compressed one as FileSize to calculate correct newCO(new current offset) on error
    if (FileGetData(path, &FileSize, 0x04, CO + 0x12) == 0) return 10; //get compressed file size
    if (FileGetData(path, &FilSize_org, 0x04, CO + 0x16) == 0) return 11; //get original file size
    if (FileSize != FilSize_org){ // if not compressed, they are same
        ErrorCode = 3; // do not return and continue to get new current offset
        snprintf(ErrorDesc, 255, "Error\nCode:3\nCO:0x%lx\n\nThe content is compressed\nCannot extract it", CO);
    }
   
    
    // Get File Name Length
    if (FileGetData(path, &FileNameLength, 0x02, CO + 0x1A) == 0) return 12;
    if (FileNameLength > 255){ //File Name Length check
        ErrorCode = 4;
        snprintf(ErrorDesc, 255, "Error\nCode:4\nCO:0x%lx\n\nContent name too long\nCannot extract it", CO);
    }
    
    // Get comment size
    if (FileGetData(path, &FileCommentSize, 0x02, CO + 0x1C) == 0) return 13;
    
    // ErrorCheck
    if (ErrorCode != 0){
        *newCO = CO + 0x1E + FileNameLength + FileCommentSize + FileSize;
        return ErrorCode;
    }
    
    // get file name
    if (FileGetData(path, FileName, FileNameLength, CO + 0x1E) == 0) return 14;
    
    // Get IsFile (by checking content name end charter)
    char ContentNameLast = FileName[strlen(FileName)-1]; // last char : '/' means it must be be a dir
    if (ContentNameLast == '/'){ // must be a directory
        ContentType = 1;
    }
    
    // get zip version to extract
    // 0x0a Default value : maybe non-compressed file
    // 0x0b Volume label : not supported
    // 0x14 Folder or compressed (Deflate) file or encrypted (PKWARE) file
    // other : encrypted or compressed or ZIP64 and not supported
    if (FileGetData(path, &ZipVersion, 0x01, CO + 0x04) == 0) return 15;
    if (ZipVersion == 0x0a){
        if (ContentType == 1){ // Mismatch (the name ends with '/', but the version to extract says it is file)
            if (FileSize != 0){ // This means the content maybe a file, so ask user
                    //↓ if silent, skip asking and handle it as a file because it must be a file
                if (silent || ShowPrompt(true, "The content maybe a directory,\nbut it maybe also a file.\nHandle as a file? or not\nRecommanded : file")){
                    ContentType = 0;
                    FileName[strlen(FileName)-1] = '\0'; // remove '/' at the end of the file name
                }
            } // else : must be just a directory and the version to extract is wrong
        }
    }else if (ZipVersion == 0x14){
        if (ContentType == 0){
            // The archive is compressed or encrypted. Can't extract.
            *newCO = CO + 0x1E + FileNameLength + FileCommentSize + FileSize;
            snprintf(ErrorDesc, 255, "Error\nCode:6\nCO:0x%lx\n\nThe content is compressed or encrypted\nCannot extract it", CO);
            return 6;
        }
    }else{
        // The archive is encrypted, compressed or used ZIP64. Can't extract.
        *newCO = CO + 0x1E + FileNameLength + FileCommentSize + FileSize;
        snprintf(ErrorDesc, 255, "Error\nCode:7\nCO:0x%lx\n\nThe content is compressed, encrypted or used ZIP64\nCannot extract it", CO);
        return 7;
    }
    
    // get start and end offset of actually data and calculate new CO
    FileDataStart_o = CO + 0x1E + FileNameLength + FileCommentSize; // comment field will be ignored
    FileDataEnd_o = FileDataStart_o + FileSize;
    *newCO = FileDataEnd_o; // File data end is start of next header
    
    // search for last '/' in content full name
    int slash_l = -1;
    for (int count_search = strlen(FileName) - 1; count_search >= 0; count_search--){
        if (FileName[count_search] == '/'){
            slash_l = count_search;
            break;
        }
    }
    if (slash_l == -1){ // no slash
        snprintf(RealFileName, 256, "%s", FileName);
        snprintf(extrpath_l, 256, "%s", extrpath);
    }else{ // slash found
        strncpy(FileinDir, FileName, slash_l);
        strncpy(RealFileName, FileName + slash_l + 1, 256);
        snprintf(extrpath_l, 256, "%s/%s", extrpath, FileinDir);
    }
    
    snprintf(File_extr_f, 256, "%s/%s", extrpath, FileName);
    fvx_rmkdir(extrpath_l); // Create Folder (do with both file and folder)
    if (ContentType == 0){ // if file, create file and inject data
        if (PathExist(File_extr_f)){ // Path already exists
            if (*flags & SKIP_ALL){ // skip all
                return 0;
            }
            if (flags && !(*flags & (OVERWRITE_CUR|OVERWRITE_ALL))){ // not overwrite
                const char* optionstr[5] =
                    {"Choose new name", "Overwrite it", "Skip it", "Overwrite all", "Skip all"};
                u32 user_select = ShowSelectPrompt(5, optionstr,
                    "Path already exists:\n%s", RealFileName);
                if (user_select == 1) {
                    do {
                        if (!ShowStringPrompt(RealFileName, 255 - (strlen(extrpath_l)), "Choose new name")){
                            return 0; // Abort will be same as "skip"
                        }
                        snprintf(File_extr_f, 256, "%s/%s", extrpath_l, RealFileName); // update path (only used one below here)
                    } while (PathExist(File_extr_f));
                } else if (user_select == 2) {
                    *flags |= OVERWRITE_CUR;
                } else if (user_select == 3) {
                    return 0;
                } else if (user_select == 4) {
                    *flags |= OVERWRITE_ALL;
                } else if (user_select == 5) {
                    *flags |= SKIP_ALL;
                    return 0;
                } else {
                    return 20; // Cancell will lead extraction to fail
                }
            }else if (*flags & (OVERWRITE_CUR)){
                *flags &= ~OVERWRITE_CUR; // overwrite current but do not since next
            }
            PathDelete(File_extr_f);
        }
        
        if (!FileCreateDummy(extrpath_l, RealFileName, FileSize)) return 16;
        if (ArchiveSize < FileDataEnd_o) return 18; // Check archive size to prevent show error in FileInjectFile
        if (!FileInjectFile(File_extr_f, path, 0, FileDataStart_o, FileSize, NULL)) return 17;
    }
    return 0; // successed
}




bool ZipExtract(const char* path, const char* extrpath, u32* flags){
    
    bool silent = (flags && (*flags & SILENT));
    
    u32 CO = 0; // Cureent offset
    u32 ErrorCode = 0; // Error code
    char ErrorDesc[256]; // Error description
    u32 countContent = 0;
    
    if (!PathExist(path)){
        if (!silent) ShowPrompt(false, "Error\nCode:0\nArchive does not exist");
        return false;
    }
    
    
    u32 Temp = 0;
    if (FileGetData(path, &Temp, 0x04, 0) == 0) return false;
    if (Temp != ZIPLFH){ // not a zip file
            if (!silent) ShowPrompt(false, "Error\nCode:5\nHeader incorrect:%lx", Temp);
            return false;
    }
    
    // make sure there is a directory to extract
    if (!CheckWritePermissions(extrpath)) return false;
    fvx_rmkdir(extrpath);
    
    // Extraction loop
    while (true){ // Loop ends with file i/o error (means reached the end of the file)
        countContent++;
        
        // Local File Header check
        if (FileGetData(path, &Temp, 0x04, CO) == 0) return true; // expected read error : it must mean reached the end of the file
        if (Temp != ZIPLFH){ // not a LFH header
            if (Temp == 0x4b500003){ // data descriptor is used
                if (!silent) ShowPrompt(false, "Error\nCode:8\nData descriptor is used.\nCannot extract.");
                return false;
            }
            return true;
        }
        ErrorCode = ZipExtractContent(path, extrpath, CO, &CO, ErrorDesc, flags);
        if (ErrorCode >= 10 && ErrorCode <= 19){ // Unexpected file read error
            if (!silent) ShowPrompt(false, "Error\nLoop:%u\nCO:%x\nCode:%u\nFile i/o error", countContent, CO, ErrorCode);
            return true;
        }else if (ErrorCode == 20){ // user cancelled
            return false;
        }else if (ErrorCode != 0){ // continuable error
            if (!silent){
                if (!ShowPrompt(true, "%s\n\nContinue to nexu content?", ErrorDesc)) break;
            }
        }
    }
    
    return true;
}
