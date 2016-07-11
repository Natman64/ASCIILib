#pragma once

#include <fstream>
#include <string>
#include <sstream>
#include <deque>
using namespace std;

#include "unicode/utypes.h"
#include "unicode/unistr.h"
using namespace icu;

namespace ascii
{

    // Reads a Unicode file line by line or all at once. Will print error messages
    // if a nonexistent file is opened, and will simplify file input code by
    // automatically closing the file read. Returns UTF-8 encoded std::strings and
    // ICU UnicodeStrings
    class FileReader
    {
        public:
            // Construct a FileReader of the given filepath.
            FileReader(string path);
            // Construct a FileReader of the given filepath checking for
            // forbidden characters defined in the second given file
            FileReader(string path, string forbiddenCharactersPath);

            // Check if the file was successfully opened
            inline bool Exists() { return mExists; }

            // Check if the file contains another line to read.
            bool HasNextLine();
            // Retrieve the next line from the file being read, UTF-8 encoded.
            string NextLine(bool trimmed=true);
            // Retrieve the next line from the file being read as
            // a UnicodeString 
            UnicodeString NextLineUnicode(bool trimmed=true);

            // Return a string containing all text in the file being read.
            string FullContents();

        private:
            void Initialize(string path);
            // Retrieve the full UTF-8 contents
            UnicodeString ReadContents(string path);
            // Parse the UTF-8 contents of a file into individual lines of text
            void ParseLines(UnicodeString contents, string path);

            deque<UnicodeString> mLines;
            bool mExists;
            UnicodeString mForbiddenCharacters;
    };

}
