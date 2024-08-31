#include "NJScanner.h"

long intFrom16BaseString(String sourceString)
{
    return strtol(sourceString.c_str(), NULL, 16);
}

int utf8length(String sourceString)
{
    const char *cstr = sourceString.c_str();
    int count = 0;
    while (*cstr != '\0')
    {
        if ((*cstr & 0xC0) != 0x80)
        {
            count++;
        }
        cstr++;
    }
    return count;
}
int numberOfComponentsWithDelimiter(String sourceString, String delimiter)
{
    int index = 0;
    int count = 1;
    do
    {
        index = sourceString.indexOf(delimiter, index);
        if (index < 0)
            break;
        count++;
        index += delimiter.length();
    } while (index < sourceString.length());

    return count;
}
String componentAtPositionWithDelimiter(String sourceString, int position, String delimiter)
{
    int prevIndex = 0;
    int index = 0;
    int componentIndex = 0;
    do
    {
        index = sourceString.indexOf(delimiter, prevIndex);
        if (index < 0)
        {
            if (componentIndex == position)
            {
                return sourceString.substring(prevIndex);
            }
            break;
        }
        if (componentIndex == position)
        {
            return sourceString.substring(prevIndex, index);
        }
        componentIndex++;
        index += delimiter.length();
        prevIndex = index;
    } while (index < sourceString.length());
    return sourceString;
}
String tagsRemovedString(String sourceString)
{
    sourceString.replace("<br>", "\n");
    NJScanner scanner = NJScanner(sourceString);
    String result = "";
    while (!scanner.isAtEnd())
    {
        if (scanner.scanString("<") > 0)
        {
            scanner.scanUpToString(">", true);
        }
        else
        {
            result += scanner.scanUpToString("<", false);
        }
    }
    return result;
}

String utf8CharStringForCodePoint(long codePoint)
{
    const uint32_t max_utf32 = 0x0010FFFF;
    const uint32_t byte_mark = 0x80;
    const uint32_t byte_mask = 0xBF;

    String result = "";

    int bytes;
    char firstByte;
    if (codePoint < 0x80) // 1000 0000
    {
        bytes = 1;
        firstByte = 0x00;
    }
    else if (codePoint < 0x800) // 1000 0000 0000
    {
        bytes = 2;
        firstByte = 0xC0;
    }
    else if (codePoint < 0x10000) // 0001 0000 0000 0000 0000
    {
        bytes = 3;
        firstByte = 0xE0;
    }
    else if (codePoint <= max_utf32)
    {
        bytes = 4;
        firstByte = 0xF0;
    }
    else
    {
        return "";
    }

    char utf8char;
    switch (bytes)
    {
    case 4:
        utf8char = (char)((codePoint | byte_mark) & byte_mask);
        result = String(utf8char) + result;
        codePoint >>= 6;

    case 3:
         utf8char = (char)((codePoint | byte_mark) & byte_mask);
        result = String(utf8char) + result;
        codePoint >>= 6;

    case 2:
         utf8char = (char)((codePoint | byte_mark) & byte_mask);
        result = String(utf8char) + result;
        codePoint >>= 6;

    case 1:
         utf8char = (codePoint | firstByte);
        result = String(utf8char) + result;
    }
    return result;
}
NJScanner::NJScanner()
{
    sourceString = "";
    sourceLocation = 0;
}

NJScanner::NJScanner(String newSourceString)
{
    sourceString = newSourceString;
    sourceLocation = 0;
}

void NJScanner::setScanString(String newSourceString)
{
    sourceString = newSourceString;
    sourceLocation = 0;
}

int NJScanner::scanString(String targetString)
{
    if (sourceString.indexOf(targetString, sourceLocation) == sourceLocation)
    {
        sourceLocation += targetString.length();
        return sourceLocation;
    }
    return -1;
}

String NJScanner::scanUpToString(String targetString, boolean skip)
{
    int index = sourceString.indexOf(targetString, sourceLocation);
    if (index > sourceLocation)
    {
        String resultString = sourceString.substring(sourceLocation, index);
        sourceLocation = index;
        if (skip)
            sourceLocation += targetString.length();
        return resultString;
    }
    String resultString = sourceString.substring(sourceLocation);
    sourceLocation = sourceString.length() - 1;
    return resultString;
}

String NJScanner::scanStringToEnd()
{
    return sourceString.substring(sourceLocation);
}

int NJScanner::scanLocation()
{
    return sourceLocation;
}
void NJScanner::setScanLocation(int newLocation)
{
    if (newLocation < sourceString.length())
    {
        sourceLocation = newLocation;
    }
    else if (newLocation < 0)
    {
        newLocation = -1;
    }
    else
    {
        newLocation = sourceString.length() - 1;
    }
}

boolean NJScanner::isAtEnd()
{
    return (sourceLocation + 1 >= sourceString.length());
}
