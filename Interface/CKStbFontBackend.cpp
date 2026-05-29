#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "CKStbFontBackend.h"

#include "VxWindowFunctions.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
static XString ExpandHomePath(const char *path)
{
    if (!path || !*path)
        return XString();

    if (path[0] != '~')
        return XString(path);

    const char *home = getenv("HOME");
    if (!home || !*home)
        return XString();

    XString expanded(home);
    expanded << (path + 1);
    return expanded;
}

static void AddDirectory(XClassArray<XString> &dirs, const XString &dir)
{
    if (dir.Length() == 0)
        return;
    if (dirs.Find(dir) == dirs.End())
        dirs.PushBack(dir);
}

static void AddPathList(XClassArray<XString> &dirs, const char *pathList, const char *suffix)
{
    if (!pathList || !*pathList)
        return;

    const char *start = pathList;
    while (*start)
    {
        const char *end = strchr(start, ':');
        XString item(start, end ? (int)(end - start) : (int)strlen(start));
        if (item.Length() > 0)
        {
            if (suffix && *suffix)
                item << suffix;
            AddDirectory(dirs, item);
        }
        if (!end)
            break;
        start = end + 1;
    }
}

static XString NormalizeName(const XString &name)
{
    XString out;
    for (int i = 0; i < name.Length(); ++i)
    {
        unsigned char c = (unsigned char)name[i];
        if (isalnum(c))
            out << (char)tolower(c);
    }
    return out;
}

static XBOOL HasFontExtension(const char *path)
{
    if (!path)
        return FALSE;

    const char *dot = strrchr(path, '.');
    if (!dot)
        return FALSE;

    XString ext(dot);
    return ext.ICompare(".ttf") == 0 ||
           ext.ICompare(".ttc") == 0 ||
           ext.ICompare(".otf") == 0;
}

static XBOOL ReadBinaryFile(const char *path, XArray<XBYTE> &data)
{
    data.Clear();
    if (!path || !*path)
        return FALSE;

    FILE *file = fopen(path, "rb");
    if (!file)
        return FALSE;

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return FALSE;
    }

    long size = ftell(file);
    if (size <= 0)
    {
        fclose(file);
        return FALSE;
    }
    rewind(file);

    data.Resize((int)size);
    size_t read = fread(data.Begin(), 1, (size_t)size, file);
    fclose(file);

    if (read != (size_t)size)
    {
        data.Clear();
        return FALSE;
    }

    return TRUE;
}

static XString DecodeUtf16BE(const unsigned char *data, int length)
{
    XString out;
    for (int i = 0; i + 1 < length; i += 2)
    {
        unsigned int code = ((unsigned int)data[i] << 8) | data[i + 1];
        if (code == 0)
            continue;
        if (code < 0x80)
        {
            out << (char)code;
        }
        else if (code < 0x800)
        {
            out << (char)(0xC0 | (code >> 6));
            out << (char)(0x80 | (code & 0x3F));
        }
        else
        {
            out << (char)(0xE0 | (code >> 12));
            out << (char)(0x80 | ((code >> 6) & 0x3F));
            out << (char)(0x80 | (code & 0x3F));
        }
    }
    return out;
}

static XString DecodeNameString(const char *data, int length, int platformId)
{
    if (!data || length <= 0)
        return XString();

    const unsigned char *bytes = (const unsigned char *)data;
    if (platformId == 0 || platformId == 3)
        return DecodeUtf16BE(bytes, length);

    XString out;
    for (int i = 0; i < length; ++i)
    {
        if (data[i])
            out << data[i];
    }
    return out;
}

static XString GetFontName(const stbtt_fontinfo &font, int nameId)
{
    struct Request
    {
        int platformId;
        int encodingId;
        int languageId;
    };

    static const Request requests[] = {
        {3, 1, 0x409},
        {3, 10, 0x409},
        {1, 0, 0},
        {0, 0, 0},
    };

    for (int i = 0; i < (int)(sizeof(requests) / sizeof(requests[0])); ++i)
    {
        int length = 0;
        const char *name = stbtt_GetFontNameString(&font, &length,
                                                   requests[i].platformId,
                                                   requests[i].encodingId,
                                                   requests[i].languageId,
                                                   nameId);
        XString decoded = DecodeNameString(name, length, requests[i].platformId);
        if (decoded.Length() > 0)
            return decoded;
    }

    return XString();
}

static XString FileStem(const char *path)
{
    if (!path)
        return XString();

    const char *name = strrchr(path, '/');
    const char *altName = strrchr(path, '\\');
    if (!name || (altName && altName > name))
        name = altName;
    name = name ? name + 1 : path;

    const char *dot = strrchr(name, '.');
    return XString(name, dot && dot > name ? (int)(dot - name) : 0);
}

static int WeightFromStyle(const XString &style, const XString &path)
{
    XString combined(style);
    combined << " ";
    combined << path;
    XString haystack = NormalizeName(combined);

    if (haystack.Find("thin") != XString::NOTFOUND)
        return 100;
    if (haystack.Find("extralight") != XString::NOTFOUND || haystack.Find("ultralight") != XString::NOTFOUND)
        return 200;
    if (haystack.Find("light") != XString::NOTFOUND)
        return 300;
    if (haystack.Find("medium") != XString::NOTFOUND)
        return 500;
    if (haystack.Find("semibold") != XString::NOTFOUND || haystack.Find("demibold") != XString::NOTFOUND)
        return 600;
    if (haystack.Find("extrabold") != XString::NOTFOUND || haystack.Find("ultrabold") != XString::NOTFOUND)
        return 800;
    if (haystack.Find("black") != XString::NOTFOUND || haystack.Find("heavy") != XString::NOTFOUND)
        return 900;
    if (haystack.Find("bold") != XString::NOTFOUND)
        return 700;
    return 400;
}

static XBOOL StyleIsItalic(const XString &style, const XString &path)
{
    XString combined(style);
    combined << " ";
    combined << path;
    XString haystack = NormalizeName(combined);
    return haystack.Find("italic") != XString::NOTFOUND ||
           haystack.Find("oblique") != XString::NOTFOUND;
}

static void AddFontFileFaces(const char *path, XClassArray<CKStbSystemFontFace> &faces)
{
    XArray<XBYTE> data;
    if (!ReadBinaryFile(path, data))
        return;

    int fontCount = stbtt_GetNumberOfFonts(data.Begin());
    if (fontCount <= 0)
        fontCount = 1;

    for (int i = 0; i < fontCount; ++i)
    {
        int offset = stbtt_GetFontOffsetForIndex(data.Begin(), i);
        if (offset < 0)
            continue;

        stbtt_fontinfo font;
        if (!stbtt_InitFont(&font, data.Begin(), offset))
            continue;

        CKStbSystemFontFace face;
        face.family = GetFontName(font, 16);
        if (face.family.Length() == 0)
            face.family = GetFontName(font, 1);
        if (face.family.Length() == 0)
            face.family = FileStem(path);

        face.style = GetFontName(font, 17);
        if (face.style.Length() == 0)
            face.style = GetFontName(font, 2);

        face.path = path;
        face.fontIndex = i;
        face.weight = WeightFromStyle(face.style, face.path);
        face.italic = StyleIsItalic(face.style, face.path);
        faces.PushBack(face);
    }
}

static int AbsInt(int value)
{
    return value < 0 ? -value : value;
}

static int FontScore(const CKStbSystemFontFace &face, const XString &normalizedFamily, int weight, XBOOL italic)
{
    int score = 0;
    if (NormalizeName(face.family) != normalizedFamily)
        score += 10000;
    score += AbsInt(face.weight - weight);
    if ((face.italic != FALSE) != (italic != FALSE))
        score += 500;
    return score;
}

static XBOOL FindByNormalizedFamilies(const XClassArray<CKStbSystemFontFace> &faces,
                                       const XClassArray<XString> &families,
                                       int weight,
                                       XBOOL italic,
                                       CKStbSystemFontFace &result)
{
    XBOOL found = FALSE;
    int bestScore = 0;
    CKStbSystemFontFace best;

    for (int i = 0; i < families.Size(); ++i)
    {
        XString normalized = NormalizeName(families[i]);
        for (int j = 0; j < faces.Size(); ++j)
        {
            if (NormalizeName(faces[j].family) != normalized)
                continue;

            int score = FontScore(faces[j], normalized, weight, italic);
            if (!found || score < bestScore)
            {
                found = TRUE;
                bestScore = score;
                best = faces[j];
            }
        }
        if (found)
        {
            result = best;
            return TRUE;
        }
    }

    return FALSE;
}

static void AddAliasFamilies(const char *family, XClassArray<XString> &aliases)
{
    aliases.Clear();
    if (family && *family)
        aliases.PushBack(XString(family));

    XString normalized = family ? NormalizeName(XString(family)) : XString();
    if (normalized == "arial" || normalized == "helvetica")
    {
        aliases.PushBack(XString("Helvetica"));
        aliases.PushBack(XString("Arial"));
        aliases.PushBack(XString("Liberation Sans"));
        aliases.PushBack(XString("DejaVu Sans"));
        aliases.PushBack(XString("Noto Sans"));
        aliases.PushBack(XString("Droid Sans"));
        aliases.PushBack(XString("Roboto"));
    }
    else if (normalized == "times" || normalized == "timesnewroman")
    {
        aliases.PushBack(XString("Times New Roman"));
        aliases.PushBack(XString("Times"));
        aliases.PushBack(XString("Liberation Serif"));
        aliases.PushBack(XString("DejaVu Serif"));
        aliases.PushBack(XString("Noto Serif"));
    }
    else if (normalized == "courier" || normalized == "couriernew")
    {
        aliases.PushBack(XString("Courier New"));
        aliases.PushBack(XString("Courier"));
        aliases.PushBack(XString("Liberation Mono"));
        aliases.PushBack(XString("DejaVu Sans Mono"));
        aliases.PushBack(XString("Droid Sans Mono"));
        aliases.PushBack(XString("Noto Sans Mono"));
        aliases.PushBack(XString("Roboto Mono"));
    }
}

static int CompareFaces(const void *lhs, const void *rhs)
{
    const CKStbSystemFontFace &a = *(const CKStbSystemFontFace *)lhs;
    const CKStbSystemFontFace &b = *(const CKStbSystemFontFace *)rhs;

    int cmp = a.family.Compare(b.family);
    if (cmp != 0)
        return cmp;
    if (a.weight != b.weight)
        return a.weight < b.weight ? -1 : 1;
    if (a.italic != b.italic)
        return a.italic ? 1 : -1;
    return a.path.Compare(b.path);
}

static void SortFaces(XClassArray<CKStbSystemFontFace> &faces)
{
    for (int i = 1; i < faces.Size(); ++i)
    {
        CKStbSystemFontFace face = faces[i];
        int j = i - 1;
        while (j >= 0 && CompareFaces(&face, &faces[j]) < 0)
        {
            faces[j + 1] = faces[j];
            --j;
        }
        faces[j + 1] = face;
    }
}

struct FontScanContext
{
    XClassArray<CKStbSystemFontFace> *faces;
    XClassArray<XString> *visitedFiles;
    XString directory;
    int depth;
};

static XString MakeFontChildPath(const XString &directory, const char *name)
{
    XString path(directory);
    if (path.Length() > 0) {
        char last = path.CStr()[path.Length() - 1];
        if (last != '/' && last != '\\')
            path << "/";
    }
    path << name;
    return path;
}

static void ScanFontDirectory(const XString &directory,
                              XClassArray<CKStbSystemFontFace> &faces,
                              XClassArray<XString> &visitedFiles,
                              int depth);

static XBOOL ScanFontDirectoryEntry(const VxDirectoryEntry *entry, void *userData)
{
    FontScanContext *context = (FontScanContext *)userData;
    if (!entry || !context)
        return FALSE;

    XString path = MakeFontChildPath(context->directory, entry->Name.CStr());

    if (entry->IsDirectory)
    {
        ScanFontDirectory(path, *context->faces, *context->visitedFiles, context->depth + 1);
        return TRUE;
    }

    if (!HasFontExtension(path.CStr()))
        return TRUE;
    if (context->visitedFiles->Find(path) != context->visitedFiles->End())
        return TRUE;

    context->visitedFiles->PushBack(path);
    AddFontFileFaces(path.CStr(), *context->faces);
    return TRUE;
}

static void ScanFontDirectory(const XString &directory,
                              XClassArray<CKStbSystemFontFace> &faces,
                              XClassArray<XString> &visitedFiles,
                              int depth)
{
    if (directory.Length() == 0 || depth > 24 || !VxDirectoryExists(directory.CStr()))
        return;

    FontScanContext context;
    context.faces = &faces;
    context.visitedFiles = &visitedFiles;
    context.directory = directory;
    context.depth = depth;
    VxListDirectory(directory.CStr(), "*", TRUE, ScanFontDirectoryEntry, &context);
}
}

void CKStbDefaultFontDirectories(XClassArray<XString> &directories)
{
    directories.Clear();

#if defined(__APPLE__)
    AddDirectory(directories, XString("/System/Library/Fonts"));
    AddDirectory(directories, XString("/System/Library/Fonts/Supplemental"));
    AddDirectory(directories, XString("/Library/Fonts"));
    AddDirectory(directories, ExpandHomePath("~/Library/Fonts"));
#endif

    AddDirectory(directories, XString("/usr/share/fonts"));
    AddDirectory(directories, XString("/usr/local/share/fonts"));
    AddDirectory(directories, ExpandHomePath("~/.local/share/fonts"));
    AddDirectory(directories, ExpandHomePath("~/.fonts"));

    const char *xdgDataHome = getenv("XDG_DATA_HOME");
    if (xdgDataHome && *xdgDataHome)
    {
        XString path(xdgDataHome);
        path << "/fonts";
        AddDirectory(directories, path);
    }

    AddPathList(directories, getenv("XDG_DATA_DIRS"), "/fonts");
    AddPathList(directories, getenv("FONTCONFIG_PATH"), "");
}

void CKStbEnumerateFontDirectories(const XClassArray<XString> &directories,
                                   XClassArray<CKStbSystemFontFace> &faces)
{
    faces.Clear();
    XClassArray<XString> visitedFiles;

    for (int i = 0; i < directories.Size(); ++i)
        ScanFontDirectory(directories[i], faces, visitedFiles, 0);

    SortFaces(faces);
}

void CKStbEnumerateSystemFonts(XClassArray<CKStbSystemFontFace> &faces)
{
    static XClassArray<CKStbSystemFontFace> systemFaces;
    if (systemFaces.Size() == 0)
    {
        XClassArray<XString> directories;
        CKStbDefaultFontDirectories(directories);
        CKStbEnumerateFontDirectories(directories, systemFaces);
    }
    faces = systemFaces;
}

XBOOL CKStbFindBestFontFace(const XClassArray<CKStbSystemFontFace> &faces,
                             const char *family,
                             int weight,
                             XBOOL italic,
                             CKStbSystemFontFace &result)
{
    if (faces.Size() == 0)
        return FALSE;

    XClassArray<XString> aliases;
    AddAliasFamilies(family, aliases);
    if (FindByNormalizedFamilies(faces, aliases, weight, italic, result))
        return TRUE;

    AddAliasFamilies("Arial", aliases);
    return FindByNormalizedFamilies(faces, aliases, weight, italic, result);
}
