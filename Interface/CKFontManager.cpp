/*************************************************************************/
/*	File : CKFontManager.cpp											 */
/*																		 */
/*************************************************************************/
#include "CKAll.h"
#include <float.h>
#include <limits.h>
#include <stdio.h>

#include "CKFontManager.h"

#if !(defined(_WIN32) || defined(WIN32)) && !defined(FONTMANAGER_NOSYSFONT)
#include "CKStbFontBackend.h"
#include "stb_truetype.h"
#endif

void DrawFillRectangle(CKRenderContext *dev, CKMaterial *mat, VxRect &rect, CKBOOL lighted, CKBOOL transform);

#define FONTMANAGERVERSION1 1

#define FONTSIZE128 (-5)
#define FONTSIZE256 (-10)
#define FONTSIZE512 (-20)
#define FONTSIZE1024 (-40)

const char *CKFontManager::Name = "Font Manager";

namespace
{
#if !(defined(_WIN32) || defined(WIN32)) && !defined(FONTMANAGER_NOSYSFONT)
struct StbFontHandle
{
    XArray<CKBYTE> data;
    stbtt_fontinfo font;
    CKStbSystemFontFace face;
    int pixelHeight;
    int ascent;
    int descent;
    int lineGap;
    int weight;
    CKBOOL italic;
    CKBOOL underline;
    FONT_METRIC metrics;
    FONT_ABC abc[256];
    int widths[256];
};

static CKBOOL ReadStbFontFile(const XString &path, XArray<CKBYTE> &data)
{
    data.Clear();

    FILE *file = fopen(path.CStr(), "rb");
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

static int RoundToInt(float value)
{
    return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
}

static int MaxInt(int a, int b)
{
    return a > b ? a : b;
}

static int MinInt(int a, int b)
{
    return a < b ? a : b;
}

static int AbsInt(int value)
{
    return value < 0 ? -value : value;
}

static void FillStbFontMetrics(StbFontHandle *handle)
{
    float scale = stbtt_ScaleForPixelHeight(&handle->font, static_cast<float>(handle->pixelHeight));
    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(&handle->font, &ascent, &descent, &lineGap);

    handle->ascent = RoundToInt(ascent * scale);
    handle->descent = RoundToInt(descent * scale);
    handle->lineGap = RoundToInt(lineGap * scale);

    handle->metrics.tmHeight = MaxInt(1, RoundToInt((ascent - descent + lineGap) * scale));
    handle->metrics.tmAscent = handle->ascent;
    handle->metrics.tmDescent = -handle->descent;
    handle->metrics.tmWeight = handle->weight;
    handle->metrics.tmAveCharWidth = 0;
    handle->metrics.tmMaxCharWidth = 0;

    int widthSum = 0;
    int widthCount = 0;
    for (int c = 0; c < 256; ++c)
    {
        int advance = 0, lsb = 0;
        stbtt_GetCodepointHMetrics(&handle->font, c, &advance, &lsb);

        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        stbtt_GetCodepointBitmapBox(&handle->font, c, scale, scale, &x0, &y0, &x1, &y1);

        int a = RoundToInt(lsb * scale);
        int b = MaxInt(0, x1 - x0);
        int width = MaxInt(0, RoundToInt(advance * scale));
        int cWidth = width - a - b;

        handle->abc[c].abcA = a;
        handle->abc[c].abcB = static_cast<CKDWORD>(b);
        handle->abc[c].abcC = cWidth;
        handle->widths[c] = width;

        if (c >= 32)
        {
            widthSum += width;
            ++widthCount;
            if (width > handle->metrics.tmMaxCharWidth)
                handle->metrics.tmMaxCharWidth = width;
        }
    }

    handle->metrics.tmAveCharWidth = widthCount > 0 ? widthSum / widthCount : handle->pixelHeight / 2;
}

static StbFontHandle *CreateStbFontHandle(const CKStbSystemFontFace &face, int pixelHeight, int weight, CKBOOL italic, CKBOOL underline)
{
    StbFontHandle *handle = new StbFontHandle;
    if (!ReadStbFontFile(face.path, handle->data))
    {
        delete handle;
        return NULL;
    }

    int offset = stbtt_GetFontOffsetForIndex(handle->data.Begin(), face.fontIndex);
    if (offset < 0 || !stbtt_InitFont(&handle->font, handle->data.Begin(), offset))
    {
        delete handle;
        return NULL;
    }

    handle->face = face;
    handle->pixelHeight = MaxInt(1, pixelHeight);
    handle->weight = weight;
    handle->italic = italic;
    handle->underline = underline;
    FillStbFontMetrics(handle);
    return handle;
}

static CKBYTE *FontTexturePixel(VxImageDescEx &desc, int x, int y)
{
    int pitch = desc.BytesPerLine > 0 ? desc.BytesPerLine : desc.Width * 4;
    return desc.Image + y * pitch + x * 4;
}

static void PutFontAlphaPixel(VxImageDescEx &desc, int x, int y, CKBYTE alpha)
{
    if (x < 0 || y < 0 || x >= desc.Width || y >= desc.Height || alpha == 0)
        return;

    CKBYTE *pixel = FontTexturePixel(desc, x, y);
    pixel[0] = 0xff;
    pixel[1] = 0xff;
    pixel[2] = 0xff;
    if (alpha > pixel[3])
        pixel[3] = alpha;
}

static void RenderStbFontAtlas(StbFontHandle *handle,
                               VxImageDescEx &desc,
                               int nbLine,
                               int nbCol,
                               int charCellSize,
                               CKBOOL renderControls)
{
    if (!handle || !desc.Image)
        return;

    float scale = stbtt_ScaleForPixelHeight(&handle->font, static_cast<float>(handle->pixelHeight));
    const bool embolden = handle->weight >= 600;

    for (int row = 0; row < nbLine; ++row)
    {
        for (int col = 0; col < nbCol; ++col)
        {
            int codepoint = row * nbCol + col;
            if (!renderControls && codepoint < 32)
                continue;
            if (stbtt_FindGlyphIndex(&handle->font, codepoint) == 0 && codepoint != 0)
                continue;

            int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
            stbtt_GetCodepointBitmapBox(&handle->font, codepoint, scale, scale, &x0, &y0, &x1, &y1);
            int glyphWidth = x1 - x0;
            int glyphHeight = y1 - y0;
            if (glyphWidth <= 0 || glyphHeight <= 0)
                continue;

            XArray<CKBYTE> glyph;
            glyph.Resize(glyphWidth * glyphHeight);
            stbtt_MakeCodepointBitmap(&handle->font, glyph.Begin(), glyphWidth, glyphHeight, glyphWidth, scale, scale, codepoint);

            int cellLeft = col * charCellSize;
            int cellTop = row * charCellSize;
            int baseline = cellTop + 1 + handle->ascent;
            int drawX = cellLeft + 1 + x0;
            int drawY = baseline + y0;

            if (drawX < cellLeft)
                drawX = cellLeft;

            for (int gy = 0; gy < glyphHeight; ++gy)
            {
                int italicOffset = handle->italic ? (glyphHeight - gy) / 4 : 0;
                for (int gx = 0; gx < glyphWidth; ++gx)
                {
                    CKBYTE alpha = glyph[gy * glyphWidth + gx];
                    int px = drawX + gx + italicOffset;
                    int py = drawY + gy;
                    if (px >= cellLeft && px < cellLeft + charCellSize && py >= cellTop && py < cellTop + charCellSize)
                    {
                        PutFontAlphaPixel(desc, px, py, alpha);
                        if (embolden)
                            PutFontAlphaPixel(desc, px + 1, py, alpha);
                    }
                }
            }

            if (handle->underline && codepoint >= 32)
            {
                int underlineY = MinInt(cellTop + charCellSize - 2, baseline + MaxInt(1, -handle->descent / 2));
                int advance = MaxInt(1, handle->widths[codepoint]);
                for (int x = 0; x < advance && x < charCellSize - 2; ++x)
                    PutFontAlphaPixel(desc, cellLeft + 1 + x, underlineY, 0xff);
            }
        }
    }
}
#endif

struct TextDrawCmd
{
    enum Type
    {
        CMD_TEXT,
        CMD_RECT,
        CMD_CARET
    };

    Type type;
    CKTextureFont *font;
    CKMaterial *material;
    CKDWORD options;
    VxRect clip;
    CKBOOL useClip;
    int indexStart;
    int indexCount;
    VxRect rect;
    CKBOOL lighted;
    CKBOOL transform;
    CompiledTextData *geom;
    float caretX;
    float caretY;
    float caretW;
    float caretH;
    CKDWORD caretFlags;
    CKMaterial *caretMaterial;
    float caretSize;
};

static CKBOOL RectEquals(const VxRect &a, const VxRect &b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

struct TextSortKey2D
{
    TextData *td;
    int z;
    int index;
};

struct TextSortKey3D
{
    TextData *td;
    float z;
    int index;
};

static bool TextSortKey2DLess(const TextSortKey2D &a, const TextSortKey2D &b)
{
    if (a.z != b.z)
        return a.z > b.z;
    return a.index < b.index;
}

static bool TextSortKey3DLess(const TextSortKey3D &a, const TextSortKey3D &b)
{
    if (a.z > b.z)
        return true;
    if (a.z < b.z)
        return false;
    return a.index < b.index;
}

template <typename T>
static void InsertionSort(T *items, int count, bool (*less)(const T &, const T &))
{
    for (int i = 1; i < count; ++i)
    {
        T value = items[i];
        int j = i - 1;
        while (j >= 0 && less(value, items[j]))
        {
            items[j + 1] = items[j];
            --j;
        }
        items[j + 1] = value;
    }
}
}

struct CKFontManager::TextDrawBatch
{
    CompiledTextData data;
    XArray<TextDrawCmd> cmds;

    void Reset()
    {
        data.Reset();
        cmds.Resize(0);
    }
};

static CKDWORD HashString(CKDWORD hash, const char *str);

void CKFontManager::SortTextList2D()
{
    int count = m_2DTexts.Size();
    if (count < 2)
        return;

    static XArray<TextSortKey2D> keys;
    keys.Resize(count);

    for (int i = 0; i < count; ++i)
    {
        TextData *td = m_2DTexts[i];
        TextSortKey2D &key = keys[i];
        key.td = td;
        key.index = i;
        CK2dEntity *ent = (CK2dEntity *)m_Context->GetObject(td->m_Entity);
        key.z = ent ? ent->GetZOrder() : INT_MIN;
    }

    InsertionSort(keys.Begin(), count, TextSortKey2DLess);

    for (int i = 0; i < count; ++i)
        m_2DTexts[i] = keys[i].td;
}

void CKFontManager::SortTextList3D(CKRenderContext *dev)
{
    if (!dev)
        return;

    int count = m_3DTexts.Size();
    if (count < 2)
        return;

    CK3dEntity *viewpoint = dev->GetViewpoint();
    if (!viewpoint)
        return;

    static XArray<TextSortKey3D> keys;
    keys.Resize(count);

    for (int i = 0; i < count; ++i)
    {
        TextData *td = m_3DTexts[i];
        TextSortKey3D &key = keys[i];
        key.td = td;
        key.index = i;
        CK3dEntity *ent = (CK3dEntity *)m_Context->GetObject(td->m_Entity);
        if (ent)
        {
            VxVector pos;
            VxVector view;
            ent->GetPosition(&pos);
            viewpoint->InverseTransform(&view, &pos);
            key.z = view.z;
        }
        else
        {
            key.z = -FLT_MAX;
        }
    }

    InsertionSort(keys.Begin(), count, TextSortKey3DLess);

    for (int i = 0; i < count; ++i)
        m_3DTexts[i] = keys[i].td;
}

// Post render callback
CKFontManager::CKFontManager(CKContext *ctx) : CKBaseManager(ctx, FONT_MANAGER_GUID, Name)
{
	ctx->RegisterNewManager(this);

    m_EntityUnderMouse = NULL;
	m_FontAttribute = 0;
	m_TextureLoaded = FALSE;
	m_VersionUpdate = FALSE;
	m_TextDrawBatch2D = new TextDrawBatch;
	m_TextBatchCmds2D = 0;
	m_TextBatchTextCmds2D = 0;
	m_TextBatchCaretCmds2D = 0;
	m_TextBatchRectCmds2D = 0;
	m_TextBatchIndices2D = 0;

#if (defined(_WIN32) || defined(WIN32)) && !defined(FONTMANAGER_NOSYSFONT)
	// System font device context initialisation
	m_DC = ::CreateCompatibleDC(NULL);
	if (m_DC == NULL)
		throw "Font Manager -- Unable to create a screen compatible DC";
#elif !defined(FONTMANAGER_NOSYSFONT)
	m_SelectedFont = NULL;
#endif
}

CKFontManager::~CKFontManager()
{
	for (CKTextureFont **it = m_FontArray.Begin(); it != m_FontArray.End(); it++)
	{
		CKTextureFont *font = *it;
		DeleteFontHandle(font->m_FontName);
		delete font;
	}

	m_FontArray.Clear();

	ClearTextData();
	delete m_TextDrawBatch2D;
	m_TextDrawBatch2D = NULL;

#if (defined(_WIN32) || defined(WIN32)) && !defined(FONTMANAGER_NOSYSFONT)
	// Release memory device context
	::DeleteDC(m_DC);
#endif
}

void CKFontManager::DeleteFont(int fontIndex)
{
	CKTextureFont *font = GetFont(fontIndex);
	if (!font)
		return;

	m_FontArray.RemoveAt(fontIndex - 1);
	DeleteFontHandle(font->m_FontName);
	CKTexture *tex = font->GetFontTexture();
	if (tex->IsDynamic())
	{
		// we now search if the texture is used anywhere else
		CKTextureFont **it = m_FontArray.Begin();
		for (; it != m_FontArray.End(); it++)
		{
			if ((*it)->GetFontTexture() == tex)
				break;
		}

		if (it == m_FontArray.End()) // Not used : we delete it
		{
			CKDestroyObject(tex);
		}
	}
	delete font;

	RebuildFontEnumeration();
}

// Delete the font data
void CKFontManager::ClearFontData()
{
	// delete the texture font and the font handle if exists
	for (CKTextureFont **it = m_FontArray.Begin(); it != m_FontArray.End();)
	{
		CKTextureFont *font = *it;
		CKTexture *texture = (CKTexture *)m_Context->GetObject(font->m_FontTexture);
		if (!texture || texture->IsDynamic())
		{
			DeleteFontHandle(font->m_FontName);
			delete font;
			it = m_FontArray.Remove(it);
		}
		else
			++it;
	}
}

// Delete the text data struct
void CKFontManager::ClearTextData()
{
	// Delete the text data for multiple settings if exists
	// this tables contains only references
	m_2DTexts.Resize(0);
	m_3DTexts.Resize(0);
	// the real data are here, we delete them
	for (TextDataTableIterator textIt = m_Texts.Begin(); textIt != m_Texts.End(); textIt++)
		delete *textIt;
	m_Texts.Clear();
}

CKERROR CKFontManager::PostClearAll()
{
	RegisterAttributes();
	ClearFontData();
	ClearTextData();

#ifndef FONTMANAGER_NOSYSFONT
	// Delete all font objects...
	FontIterator it = m_Fonts.Begin();
	FontIterator fend = m_Fonts.End();

	// And we release the handle
	while (it != fend)
	{
#if defined(_WIN32) || defined(WIN32)
		::DeleteObject(*it);
#else
		if (m_SelectedFont == *it)
			m_SelectedFont = NULL;
		delete static_cast<StbFontHandle *>(*it);
#endif
		++it;
	}
	m_Fonts.Clear();
#endif

	return CK_OK;
}

void CKFontManager::RegisterAttributes()
{
	CKParameterManager *pm = m_Context->GetParameterManager();
	pm->RegisterNewEnum(CKPGUID_FONT, "Font Type", "No Font=0");
	pm->ChangeEnumDeclaration(CKPGUID_FONT, "No Font=0");
	CKParameterTypeDesc *pt = pm->GetParameterTypeDescription(CKPGUID_FONT);
	pt->Saver_Manager = FONT_MANAGER_GUID;
}

CKERROR CKFontManager::OnCKInit()
{
	RegisterAttributes();
	RegenerateFontEnumeration();

	return CK_OK;
}

CKERROR CKFontManager::OnCKEnd()
{
	return CK_OK;
}

CKERROR CKFontManager::OnPostRender(CKRenderContext *dev)
{
	DrawTextCallback3D(dev, this);

	return CK_OK;
}

CKERROR CKFontManager::OnPostSpriteRender(CKRenderContext *dev)
{
	// 2.5 change
	// changes for text to appear after the 2D entities
	DrawTextCallback2D(dev, this);

	DrawRectanglesCallback(dev);
	return CK_OK;
}

CKERROR CKFontManager::PreProcess()
{
    // Clear the reference table for next use
    m_2DTexts.Resize(0);
    m_3DTexts.Resize(0);
    m_Rectangles.Resize(0);

	///
	// we need to find the 2DEntity under the mouse cursor
	m_EntityUnderMouse = NULL;

	CKInputManager *im = (CKInputManager *)m_Context->GetManagerByGuid(INPUT_MANAGER_GUID);
	if (!im)
		return CK_OK;

	Vx2DVector mp;
	im->GetMousePosition(mp, FALSE);

	CKRenderContext *rc = m_Context->GetPlayerRenderContext();
	if (rc)
	{
		VxRect screen;
		rc->GetWindowRect(screen, FALSE);
		mp += screen.GetTopLeft();
		m_EntityUnderMouse = rc->Pick2D(mp);
	}

	return CK_OK;
}

CKERROR CKFontManager::OnCKReset()
{
	ClearFontData();
	ClearTextData();
	return CK_OK;
}

/*************************************************
Name: SequenceToBeDeleted

Summary: System font could be created from dynamical texture.
We must destroy these font if the texture is deleted

Arguments: the objects deleted

Return Value:

Remarks:

See also:

*************************************************/
CKERROR CKFontManager::SequenceToBeDeleted(CK_ID *objIds, int count)
{
	// Check the texture
	OnTextureToBeDeleted();

	// We check the compiled data
	for (CompiledTextTableIterator compIt = m_CompiledText.Begin(); compIt != m_CompiledText.End();)
	{
		CKObject *obj = m_Context->GetObject(compIt.GetKey());
		if (!obj || obj->IsToBeDeleted())
		{
			delete *compIt;
			compIt = m_CompiledText.Remove(compIt);
		}
		else
			compIt++;
	}

	// check the 3D texts
	TextData **td;
	for (td = m_3DTexts.Begin(); td != m_3DTexts.End();)
	{
		CK3dEntity *ent = (CK3dEntity *)m_Context->GetObject((*td)->m_Entity);
		if (!ent || ent->IsToBeDeleted())
		{
			td = m_3DTexts.Remove(td);
		}
		else
			++td;
	}

	// check the 2D texts
	for (td = m_2DTexts.Begin(); td != m_2DTexts.End();)
	{
		CK2dEntity *ent = (CK2dEntity *)m_Context->GetObject((*td)->m_Entity);
		if (!ent || ent->IsToBeDeleted())
		{
			td = m_2DTexts.Remove(td);
		}
		else
			++td;
	}

	// We check the text Data
	for (TextDataTableIterator textIt = m_Texts.Begin(); textIt != m_Texts.End();)
	{
		CKObject *obj = m_Context->GetObject(textIt.GetKey());
		if (!obj || obj->IsToBeDeleted())
		{
			delete *textIt;
			textIt = m_Texts.Remove(textIt);
		}
		else
			textIt++;
	}

	return CK_OK;
}

// Conveniences for SequenceToBeDeleted
void CKFontManager::OnTextureToBeDeleted()
{
	int index = 0;
	CKTextureFont **it = m_FontArray.Begin();
	while (it != m_FontArray.End())
	{
		CKTextureFont *font = *it;
		CKTexture *tex = (CKTexture *)m_Context->GetObject(font->m_FontTexture);
		if (tex && tex->IsToBeDeleted())
		{
			// the texture is destroyed : delete the associated logical font if any
			DeleteFontHandle(font->m_FontName);
			// we delete the texture
			delete font;
			// and we remove it from the array
			it = m_FontArray.Remove(it);

			// We update the local parameters using this deleted texture
			CK_ID *ids = m_Context->GetObjectsListByClassID(CKCID_PARAMETERLOCAL);
			int j, idCount = m_Context->GetObjectsCountByClassID(CKCID_PARAMETERLOCAL);

			for (j = 0; j < idCount; ++j)
			{
				CKParameterLocal *pl = (CKParameterLocal *)m_Context->GetObject(ids[j]);
				if (pl && (pl->GetGUID() == CKPGUID_FONT))
				{
					int *value = (int *)pl->GetWriteDataPtr();
					if (*value == (index + 1))
						*value = 0;
					else if (*value > (index + 1))
						(*value)--;
				}
			}

			ids = m_Context->GetObjectsListByClassID(CKCID_PARAMETEROUT);
            idCount = m_Context->GetObjectsCountByClassID(CKCID_PARAMETEROUT);

			for (j = 0; j < idCount; ++j)
			{
				CKParameterOut *pl = (CKParameterOut *)m_Context->GetObject(ids[j]);
				if (pl && (pl->GetGUID() == CKPGUID_FONT))
				{
					int *value = (int *)pl->GetWriteDataPtr();
					if (*value == (index + 1))
						*value = 0;
					else if (*value > (index + 1))
						(*value)--;
				}
			}
		}
		else
		{
			++it;
			++index;
		}
	}
}

/*************************************************
Name: RebuildFontEnumeration

Summary: Build the enumeration representing the available fonts

Arguments:

Return Value:

Remarks: Called at font creation or font deletion

See also:

*************************************************/
void CKFontManager::RebuildFontEnumeration()
{
	XString xenumstring;
	XString xtmp;

	int index = 1;
	// Scan all available fonts
	for (CKTextureFont **it = m_FontArray.Begin(); it != m_FontArray.End(); it++, ++index)
	{
		CKTextureFont *font = *it;
		xtmp.Format("%s=%d", font->m_FontName, index);
		xenumstring += xtmp;
		if (index != m_FontArray.Size())
			xenumstring += ",";
	}

	if (!xenumstring.Length())
		xenumstring = "No Font=0";

	// Register the new enumeration
	m_Context->GetParameterManager()->ChangeEnumDeclaration(CKPGUID_FONT, xenumstring.Str());
}

/*************************************************
Name: GetFontIndex

Summary: Get the index of a font from its name

Arguments:

Return Value:

Remarks:

See also:

*************************************************/
int CKFontManager::GetFontIndex(CKSTRING name)
{
	if (!name)
		return 0;

	int index = 1;

	for (CKTextureFont **it = m_FontArray.Begin(); it != m_FontArray.End(); it++, index++)
	{
		CKTextureFont *font = *it;
		if (font->m_FontName)
			if (font && !strcmp(font->m_FontName, name))
				return index;
	}

	return 0;
}

/*************************************************
Name: RegisterFont

Summary: Add a new font to the font manager

Arguments: a CKTextureFont representing the font

Return Value:

Remarks:

See also:

*************************************************/
int CKFontManager::RegisterFont(CKTextureFont *font)
{
	// Add to the font array
	m_FontArray.PushBack(font);

	// Change the enum
	RebuildFontEnumeration();

	// return Index+1
	return m_FontArray.Size();
}

int CKFontManager::CreateTextureFont(CKSTRING fontName, CKTexture *fontTexture, VxRect &textZone, Vx2DVector &charNumber, CKBOOL fixed, int firstCharacter, float iSpaceSize)
{
	if (!fontTexture)
		return -1;
	if (!fontName)
		return -1;

	// We create the new font
	CKTextureFont *font = new CKTextureFont(this, m_Context, fontName);
	font->CreateCKFont(fontTexture, textZone, charNumber, fixed, firstCharacter, iSpaceSize);
	font->m_ScreenExtents.Set(fontTexture->GetWidth(), fontTexture->GetHeight());

	// We look for existent
	int index = GetFontIndex(fontName);
	if (index) // the font was found.
	{
		// we delete the old one
		CKTextureFont *old = m_FontArray[index - 1];
		if (old)
		{
			DeleteFontHandle(old->m_FontName);
			CKTexture *tex = old->GetFontTexture();
			if (tex && tex->IsDynamic() && tex != fontTexture)
			{
				CKBOOL used = FALSE;
				for (CKTextureFont **it = m_FontArray.Begin(); it != m_FontArray.End(); ++it)
				{
					if (*it != old && (*it)->GetFontTexture() == tex)
					{
						used = TRUE;
						break;
					}
				}
				if (!used)
					CKDestroyObject(tex);
			}
		}
		delete old;

		// we store the new one
		m_FontArray[index - 1] = font;

		return index;
	}
	else // No similar font found : we create a new one
	{
		// We add the font to the font array
		return RegisterFont(font);
	}
}

CKTextureFont *CKFontManager::GetFont(unsigned int fontIndex)
{
	if (fontIndex <= 0)
		return NULL;
	if (fontIndex > (unsigned int)m_FontArray.Size())
		return NULL;

	CKTextureFont *tf = m_FontArray[fontIndex - 1];
	if (tf->m_SpacingProperties & CKTextureFont::CREATED)
		return tf;
	else
		return NULL;
}

// Text Lines

void CKFontManager::ClearLines()
{
	m_Lines.Resize(0);
}

void CKFontManager::AddLine(LineData &data)
{
	m_Lines.PushBack(data);
}

int CKFontManager::GetLineCount()
{
	return m_Lines.Size();
}

LineData *CKFontManager::GetLine(int i)
{
	if (i < m_Lines.Size())
	{
		return &m_Lines[i];
	}
	return NULL;
}

#define FONT_VERSION1 0x8001
#define FONT_VERSION2 0x8002

CKStateChunk *CKFontManager::SaveData(CKFile *SavedFile)
{
	if (!m_FontArray.Size())
		return NULL;

	///////////////////////////////////
	// FONTS SAVING
	//////////////////////////////////

	int textureCount = 0;
	CKTextureFont **it;
	for (it = m_FontArray.Begin(); it != m_FontArray.End(); it++)
	{
		CKTextureFont *font = *it;

		CKTexture *texture = (CKTexture *)m_Context->GetObject(font->m_FontTexture);
		if (!texture)
			continue; // does not exist
		if (texture->IsDynamic())
			continue; // does not need to be saved
		if (!SavedFile->IsObjectToBeSaved(font->m_FontTexture))
			continue; // the texture is not going to be saved

		textureCount++;
	}

	if (!textureCount)
		return NULL;

	CKStateChunk *chunk = CreateCKStateChunk((CK_CLASSID)FONTMANAGERVERSION1, SavedFile);
	chunk->StartWrite();

	// Font Identifier
	chunk->WriteIdentifier(FONT_VERSION2);

	// we write the number of fonts
	chunk->WriteInt(textureCount);

	// Fonts processing
	for (it = m_FontArray.Begin(); it != m_FontArray.End(); it++)
	{
		CKTextureFont *font = *it;
		CKTexture *texture = (CKTexture *)m_Context->GetObject(font->m_FontTexture);
		if (!texture)
			continue; // does not exist
		if (texture->IsDynamic())
			continue; // does not need to be saved
		if (!SavedFile->IsObjectToBeSaved(font->m_FontTexture))
			continue; // the texture is not going to be saved

		// FontName
		chunk->WriteString(font->m_FontName);
		// CharNumber
		chunk->WriteBuffer(sizeof(Vx2DVector), &(font->m_CharNumber));
		// FontZone
		chunk->WriteBuffer(sizeof(VxRect), &(font->m_FontZone));
		// Texture ID
		chunk->WriteObjectID(font->m_FontTexture);
		// Spacing Properties
		chunk->WriteInt(font->m_SpacingProperties & CKTextureFont::TOBESAVED);
		// First Character
		chunk->WriteInt(font->m_FirstCharacter);
		// we now maybe have to save the spacing properties for the given font
		if (font->m_SpacingProperties & CKTextureFont::SPACINGTOBESAVED)
		{
			chunk->WriteBufferNoSize_LEndian(256 * sizeof(CharacterTextureCoordinates), &(font->m_FontCoordinates));
		}
	}

	chunk->CloseChunk();
	return chunk;
}

// {secret}
CKERROR CKFontManager::LoadData(CKStateChunk *chunk, CKFile *LoadedFile)
{
	if (!chunk)
		return CKERR_INVALIDPARAMETER;

	chunk->StartRead();

	///////////////////////////////////
	// FONTS LOADING
	//////////////////////////////////

	// font Identifier
	int version = 2;
	if (!chunk->SeekIdentifier(FONT_VERSION2))
	{
		if (chunk->SeekIdentifier(FONT_VERSION1))
			version = 1;
		else
			version = 0;
	}

	if (version)
	{
		// we read the number of fonts
		int fn = chunk->ReadInt();

		int *ConversionTable = new int[fn + 1];
		// No Remapping for the first one
		memset(ConversionTable, 0, 4 * (fn + 1));

		char buffer[32];
		// we read the font
		for (int h = 0; h < fn; h++)
		{
			// Font Name
			chunk->ReadAndFillBuffer(&buffer);
			// char Number
			Vx2DVector cn;
			chunk->ReadAndFillBuffer_LEndian(&cn);
			// Font Zone
			VxRect fz;
			chunk->ReadAndFillBuffer_LEndian(&fz);
			// Texture ID
			CK_ID id = chunk->ReadObjectID();
			CKObject *obj = CKGetObject(id);
			if (!CKIsChildClassOf(obj, CKCID_TEXTURE))
				id = 0;

			// Spacing
			int spacing = chunk->ReadInt();
			// First character
			int firstCharacter = chunk->ReadInt();

			int index = GetFontIndex(buffer);
			if (!index && id) // Font does not already exist and the texture is present
			{
				// New font
				CKTextureFont *font = new CKTextureFont(this, m_Context, buffer);
				font->m_FontZone = fz;
				font->m_CharNumber = cn;

				if (version == 1)
				{
					if (spacing == 1)
						font->m_SpacingProperties = CKTextureFont::FIXED;
					else
						font->m_SpacingProperties = 0;
				}
				else // version2
				{
					font->m_SpacingProperties = spacing;

					if (spacing & CKTextureFont::SPACINGTOBESAVED)
					{ // We have saved spacing data too
						chunk->ReadAndFillBuffer_LEndian(256 * sizeof(CharacterTextureCoordinates), (&font->m_FontCoordinates));
						// the font is marked as created
						font->m_SpacingProperties |= CKTextureFont::CREATED;
					}
				}
				font->m_FontTexture = id;
				font->m_FirstCharacter = firstCharacter;

				// We add the font to the existing one
				index = RegisterFont(font);
				m_TextureLoaded = TRUE;
			}
			else // if version 2, we need to skip the spacing if they were saved
			{
				if ((version == 2) && (spacing & CKTextureFont::SPACINGTOBESAVED)) // We have saved spacing data too
				{
					chunk->Skip(256 * sizeof(CharacterTextureCoordinates) / 4);
				}
			}
			// Remapping [oldIndex] = newIndex
			ConversionTable[h + 1] = index;
		}

		///
		// Remapping
		LoadedFile->RemapManagerInt(FONT_MANAGER_GUID, ConversionTable, fn + 1);

		delete[] ConversionTable;
	}

	return CK_OK;
}

CKERROR CKFontManager::PostLoad()
{
	if (m_TextureLoaded)
	{
		// We now create the font that were only prepared by the loading
		for (CKTextureFont **it = m_FontArray.Begin(); it != m_FontArray.End(); it++)
		{
			CKTextureFont *font = *it;
			font->CreateFromTexture();
		}
		m_TextureLoaded = FALSE;
	}
	return CK_OK;
}

CompiledTextData *CKFontManager::AddCompiledText(CK_ID id)
{
	CompiledTextData *ctdata = new CompiledTextData;
	m_CompiledText.Insert(id, ctdata, TRUE);
	return ctdata;
}

void CKFontManager::RemoveCompiledText(CK_ID id)
{
	CompiledTextData *ctdata;
	if (m_CompiledText.LookUp(id, ctdata))
	{
		delete ctdata;
		m_CompiledText.Remove(id);
	}
}

CompiledTextData *CKFontManager::GetCompiledText(CK_ID id)
{
	CompiledTextData *ctdata = NULL;
	m_CompiledText.LookUp(id, ctdata);
	return ctdata;
}

void CKFontManager::DrawRectangle(const RectangleData &rd)
{
	m_Rectangles.PushBack(rd);
}

/*************************************************
Name: DrawText

Summary: Store text data in manager : used by the texts block with multiple setting checked

Arguments: entity : ID of the targeted frame (2D/3D)
		 fontIndex : index of the font to use
		 string : text to store
		 scale, endColor, align, textZone, mat, textFlags : parameter associated with data : see 2D/3D Text blocks

Return Value: none

Remarks: the data are stored in m_Texts, a reference is added to m_2DTexts or m_3DTexts
		they are used when we retrieve the data for an entity, so we don't scan all the data
		in case we use 2D and 3D texts blocks

See also:

*************************************************/
void CKFontManager::DrawText(CK_ID entity, int fontIndex, CKSTRING string, Vx2DVector scale, CKDWORD startColor, CKDWORD endColor, int align, VxRect textZone, CKMaterial *mat, int textFlags)
{
	// No text : do nothing
	if (!string)
		return;

	// If we already have a TextData struct for this entity, re-use it
	TextData *td = NULL;

	if (m_Texts.LookUp(entity, td))
	{
		// we don't use COMPILED setting
		if (!(textFlags & TEXT_COMPILED))
		{
			td->m_FontIndex = fontIndex;
			if (!td->m_String || strcmp(td->m_String, string) != 0)
			{
				CKDeletePointer(td->m_String); // The string length can change, don't overwrite directly
				td->m_String = CKStrdup(string);
				td->m_StringHash = HashString(2166136261u, td->m_String);
			}
			td->m_Scale = scale;
			td->m_StartColor = startColor;
			td->m_EndColor = endColor;
			td->m_Align = align;
			td->m_TextZone = textZone;
			td->m_Mat = mat;
			td->m_TextFlags = textFlags;
		}
		// for now, if COMPILED setting is used, we don't change the text data content
		// changes goes here
	}
	else
	{
		// No TextData for this entity, create a new structure and add it to the list
		td = new TextData(entity, fontIndex, string, scale, startColor, endColor, align, textZone, mat, textFlags);
		td->m_StringHash = HashString(2166136261u, td->m_String);
		m_Texts.Insert(entity, td, TRUE);
	}

	CKTextureFont *fn = GetFont(fontIndex);
	if (td && fn)
	{
		td->m_Margins = fn->m_Margins;
		td->m_Offset = fn->m_Offset;
		td->m_ParagraphIndentation = fn->m_ParagraphIndentation;
		td->m_CaretMaterial = fn->m_CaretMaterial;
		td->m_CaretSize = fn->m_CaretSize;
		td->m_ClippingRect = fn->m_ClippingRect;
	}

	// Store the reference in the good table, these array are cleared when render are done,
	// so we can add the data without check
	if (textFlags & TEXT_3D)
		m_3DTexts.PushBack(td);
	else
		m_2DTexts.PushBack(td);
}

void CKFontManager::DrawText(CKRenderContext *iRC, int iFontIndex, const char *iText, const Vx2DVector &iPosition, const Vx2DVector &iScale, CKDWORD iStartColor, CKDWORD iEndColor)
{
	CKTextureFont *font = NULL;
	if (iFontIndex)
	{
		font = GetFont(iFontIndex);
	}

	if (!font)
		return;

	Vx2DVector oldScale = font->m_Scale;
	CKDWORD oldStartColor = font->m_StartColor;
	CKDWORD oldEndColor = font->m_EndColor;
	float oldLeadingX = font->m_Leading.x;
	CKDWORD oldProperties = font->m_Properties;

	font->m_Scale = iScale;
	font->m_StartColor = iStartColor;
	font->m_EndColor = iEndColor;
	font->m_Leading.x = -3.0f;
	font->m_Properties |= FONT_DISABLEFILTER;

	// draw the text
	font->DrawStringEx(iRC, iText, VxRect(iPosition.x, iPosition.y, (float)iRC->GetWidth(), (float)iRC->GetHeight()), 0);

	font->m_Scale = oldScale;
	font->m_StartColor = oldStartColor;
	font->m_EndColor = oldEndColor;
	font->m_Leading.x = oldLeadingX;
	font->m_Properties = oldProperties;
}

void CKFontManager::DrawRectanglesCallback(CKRenderContext *dev)
{
	if (!m_Rectangles.Size())
		return;

	VxRect oldclip;
	dev->GetViewRect(oldclip);
	VxRect clip;
	dev->GetWindowRect(clip);
	clip.Translate(-clip.GetTopLeft());
	dev->SetViewRect(clip);

	// We draw all the rectangles
	for (int i = 0; i < m_Rectangles.Size(); ++i)
	{
		float size = m_Rectangles[i].m_BorderSize;
		if (size == 0.0f) // No Border
		{
			DrawFillRectangle(dev, m_Rectangles[i].m_InColor, m_Rectangles[i].m_Material, m_Rectangles[i].m_Screen, m_Rectangles[i].m_InUVs);
		}
		else
		{
			if (size < 0.0f) // Border
			{
				DrawBorderRectangle(dev, m_Rectangles[i].m_BorderColor, m_Rectangles[i].m_Material, m_Rectangles[i].m_Screen, -size);
			}
			else // Interior + Border
			{
				VxRect rect = m_Rectangles[i].m_Screen;
				rect.Inflate(Vx2DVector(-size, -size));
				DrawFillRectangle(dev, m_Rectangles[i].m_InColor, m_Rectangles[i].m_Material, rect, m_Rectangles[i].m_InUVs);
				DrawBorderRectangle(dev, m_Rectangles[i].m_BorderColor, m_Rectangles[i].m_Material, m_Rectangles[i].m_Screen, size);
			}
		}
	}

	dev->SetViewRect(oldclip);
}

/*************************************************
Name: parent2DHidden, parent3DHidden

Summary: look the parent of an entity to verify it's visibility

Arguments: ent : entity to check for visibility

Return Value: TRUE if a parent is hidden else FALSE

Remarks: none

See also: DrawTextCallback

*************************************************/
inline CKBOOL parent2DHidden(CK2dEntity *ent)
{
	while (ent)
	{
		if (ent->IsHierarchicallyHide())
			return TRUE;
		ent = ent->GetParent();
	}
	return FALSE;
}

inline CKBOOL parent3DHidden(CK3dEntity *ent)
{
	while (ent)
	{
		if (ent->IsHierarchicallyHide())
			return TRUE;
		ent = ent->GetParent();
	}
	return FALSE;
}

static const CKDWORD TEXT_SCISSOR = 4096;

struct TextEmitContext
{
	CKFontManager::TextDrawBatch *batch;
	CKTextureFont *font;
	CKDWORD options;
	VxRect clip;
	CKBOOL useClip;
};

static CKDWORD HashAdd(CKDWORD hash, const void *data, int size)
{
	const unsigned char *ptr = (const unsigned char *)data;
	for (int i = 0; i < size; ++i)
	{
		hash ^= ptr[i];
		hash *= 16777619u;
	}
	return hash;
}

static CKDWORD HashString(CKDWORD hash, const char *str)
{
	const unsigned char *ptr = (const unsigned char *)(str ? str : "");
	while (*ptr)
	{
		hash ^= *ptr++;
		hash *= 16777619u;
	}
	return hash;
}

static CKDWORD BuildTextCacheKey(CKTextureFont *font, TextData *td, CKRenderContext *dev)
{
	CKDWORD hash = td->m_StringHash ? td->m_StringHash : HashString(2166136261u, td->m_String);

	hash = HashAdd(hash, &td->m_FontIndex, sizeof(td->m_FontIndex));
	hash = HashAdd(hash, &td->m_Align, sizeof(td->m_Align));
	hash = HashAdd(hash, &td->m_TextZone, sizeof(td->m_TextZone));
	hash = HashAdd(hash, &td->m_Scale, sizeof(td->m_Scale));
	hash = HashAdd(hash, &td->m_StartColor, sizeof(td->m_StartColor));
	hash = HashAdd(hash, &td->m_EndColor, sizeof(td->m_EndColor));
	hash = HashAdd(hash, &td->m_Margins, sizeof(td->m_Margins));
	hash = HashAdd(hash, &td->m_Offset, sizeof(td->m_Offset));
	hash = HashAdd(hash, &td->m_ParagraphIndentation, sizeof(td->m_ParagraphIndentation));
	hash = HashAdd(hash, &td->m_TextFlags, sizeof(td->m_TextFlags));

	hash = HashAdd(hash, &font->m_Leading, sizeof(font->m_Leading));
	hash = HashAdd(hash, &font->m_ItalicOffset, sizeof(font->m_ItalicOffset));
	hash = HashAdd(hash, &font->m_Properties, sizeof(font->m_Properties));
	hash = HashAdd(hash, &font->m_ShadowColor, sizeof(font->m_ShadowColor));
	hash = HashAdd(hash, &font->m_ShadowOffset, sizeof(font->m_ShadowOffset));
	hash = HashAdd(hash, &font->m_ShadowScale, sizeof(font->m_ShadowScale));
	hash = HashAdd(hash, &font->m_FontTexture, sizeof(font->m_FontTexture));
	hash = HashAdd(hash, &font->m_FontManager->m_VersionUpdate, sizeof(font->m_FontManager->m_VersionUpdate));

	if ((td->m_TextFlags & TEXT_SCREEN) && dev)
	{
		int width = dev->GetWidth();
		int height = dev->GetHeight();
		hash = HashAdd(hash, &width, sizeof(width));
		hash = HashAdd(hash, &height, sizeof(height));
	}

	return hash;
}

static void EmitCaretCommand(void *user, float x, float y, float w, float h, CKDWORD flags)
{
	TextEmitContext *ctx = (TextEmitContext *)user;

	TextDrawCmd cmd;
	cmd.type = TextDrawCmd::CMD_CARET;
	cmd.font = ctx->font;
	cmd.material = NULL;
	cmd.options = ctx->options;
	cmd.clip = ctx->clip;
	cmd.useClip = ctx->useClip;
	cmd.indexStart = 0;
	cmd.indexCount = 0;
	cmd.rect = VxRect(0.0f, 0.0f, 0.0f, 0.0f);
	cmd.lighted = FALSE;
	cmd.transform = FALSE;
	cmd.geom = NULL;
	cmd.caretX = x;
	cmd.caretY = y;
	cmd.caretW = w;
	cmd.caretH = h;
	cmd.caretFlags = flags;
	cmd.caretMaterial = ctx->font->m_CaretMaterial;
	cmd.caretSize = ctx->font->m_CaretSize;

	ctx->batch->cmds.PushBack(cmd);
}

static void PushTextCommand(CKFontManager::TextDrawBatch &batch, const TextDrawCmd &cmd)
{
    int count = batch.cmds.Size();
    if (count > 0)
    {
        TextDrawCmd &last = batch.cmds[count - 1];
        if (last.type == TextDrawCmd::CMD_TEXT && cmd.type == TextDrawCmd::CMD_TEXT &&
            last.font == cmd.font && last.options == cmd.options && last.geom == cmd.geom &&
            last.useClip == cmd.useClip && (!cmd.useClip || RectEquals(last.clip, cmd.clip)) &&
            (last.indexStart + last.indexCount) == cmd.indexStart)
        {
            last.indexCount += cmd.indexCount;
            return;
        }
    }

    batch.cmds.PushBack(cmd);
}

static int AppendStringShadowedBatch(CKTextureFont *font, CKRenderContext *dev, CKSTRING string, int slen, VxVector position, VxRect &textZone, CKDWORD options, CompiledTextData *data, TextDrawEmitter *emitter)
{
	int indices = 0;
	if (ColorGetAlpha(font->m_ShadowColor))
	{
		VxVector shadowoffset(font->m_ShadowOffset.x, font->m_ShadowOffset.y, 0);
		VxVector shadowpos = position + shadowoffset;

		CKDWORD oldproperties = font->m_Properties;
		font->m_Properties &= ~FONT_GRADIENT;

		CKDWORD oldstartcolor = font->m_StartColor;
		font->m_StartColor = font->m_ShadowColor;

		Vx2DVector oldscale = font->m_Scale;
		font->m_Scale = font->m_ShadowScale;

		indices += font->AppendStringGeometry(dev, string, slen, shadowpos, textZone, options, data, emitter);

		font->m_StartColor = oldstartcolor;
		font->m_Properties = oldproperties;
		font->m_Scale = oldscale;
	}

	indices += font->AppendStringGeometry(dev, string, slen, position, textZone, options, data, emitter);

	return indices;
}

static CKBOOL BuildTextBatch2D(CKRenderContext *dev, CKBeObject *obj, TextData *td, CKTextureFont *font, CKFontManager::TextDrawBatch &batch, CompiledTextData *targetData)
{
	CompiledTextData *geomData = targetData ? targetData : &batch.data;
	if (targetData)
		geomData->Reset();

	if (!td || !font || !td->m_String)
		return FALSE;

	int align = td->m_Align;
	CKDWORD options = td->m_TextFlags;
	const CKBOOL doWrap = (options & (TEXT_JUSTIFIED | TEXT_WORDWRAP)) != 0;
	const CKBOOL alignLeft = (align & HALIGN_LEFT) != 0;
	const CKBOOL alignRight = (align & HALIGN_RIGHT) != 0;
	const CKBOOL alignTop = (align & VALIGN_TOP) != 0;
	const CKBOOL alignBottom = (align & VALIGN_BOTTOM) != 0;

	VxRect drawzone = td->m_TextZone;
	VxRect textZone = td->m_TextZone;
	textZone.left += font->m_Margins.left;
	textZone.right -= font->m_Margins.right;
	textZone.top += font->m_Margins.top;
	textZone.bottom -= font->m_Margins.bottom;

	// We choose the relative coordinates (screen or textures)
	if (options & TEXT_SCREEN)
	{
		font->m_ScreenExtents.Set((float)dev->GetWidth(), (float)dev->GetHeight());
	}
	else
	{
		CKTexture *fonttexture = font->GetFontTexture();
		if (fonttexture)
			font->m_ScreenExtents.Set((float)fonttexture->GetWidth(), (float)fonttexture->GetHeight());
	}

	if (doWrap)
	{
		if (textZone.GetWidth() < font->m_FontZone.GetWidth() / (font->m_Scale.x * font->m_ScreenExtents.x * font->m_CharNumber.x))
			return FALSE;
	}

	const CharacterTextureCoordinates *coords = font->m_FontCoordinates;
	float width = font->m_Scale.x * font->m_ScreenExtents.x;
	float height = font->m_Scale.y * font->m_ScreenExtents.y;
	float hlead = font->m_Leading.x;
	if (font->m_FontManager->m_VersionUpdate)
	{
		float spaceadvance = coords[' '].uprewidth + coords[' '].uwidth + coords[' '].upostwidth;
		hlead = 0.1f * width * spaceadvance * font->m_Leading.x;
	}
	float spacesize = width * (coords[' '].uprewidth + coords[' '].uwidth + coords[' '].upostwidth) + hlead;
	float zonewidth = textZone.GetWidth();
	float paragraphIndentX = font->m_ParagraphIndentation.x * coords[0].uwidth * width;
	float paragraphIndentY = font->m_ParagraphIndentation.y * coords[0].vwidth * height;
	float verticalspace = coords[0].vwidth * height + font->m_Leading.y;

	// We clear the line array
	font->m_FontManager->ClearLines();

	// Line Data Variables
	LineData ldata;

	// Wrapping variables
	const char *string = td->m_String;
	const char *lastword = "\n";
	int lastlen = 0;
	float lastwidth = 0.0f;
	CKBOOL lastcharacterwasspace = TRUE;
	CKBOOL paragraphstart = TRUE;
	int linecount = 0;
	float textwidth = 0.0f;
	float textheight = 0.0f;

	// String cutting, LineData Array filling
	while (*string)
	{
		ldata.string = string;
		ldata.len = 0;
		ldata.nbspace = 0;
		if (paragraphstart)
		{
			ldata.stringwidth = paragraphIndentX;
		}
		else
			ldata.stringwidth = 0.0f;

		// we iterate line by line
		while (*string && *string != '\n')
		{
			// We encounter a space
			if (*string == ' ')
			{
				if (!lastcharacterwasspace) // we already encounter a space
					lastcharacterwasspace = TRUE;

				lastlen = ldata.len;
				lastword = string + 1;
				lastwidth = ldata.stringwidth;

				ldata.nbspace++;
				ldata.stringwidth += spacesize;
			}
			else // we encounter a character
			{
				if (lastcharacterwasspace) // Is it a word start ?
				{
					lastcharacterwasspace = FALSE;
					lastlen = ldata.len - 1;
					lastword = string;
					lastwidth = ldata.stringwidth - spacesize;
				}
				const CharacterTextureCoordinates *tctc = &coords[(unsigned char)*string];
				ldata.stringwidth += (tctc->uprewidth + tctc->uwidth + tctc->upostwidth) * width + hlead;
			}

			// If wordwrap, whe have to see if the text fit in the line
			if (doWrap)
			{
				if (ldata.stringwidth > zonewidth) // it does not : we break the line
				{
					if (lastlen > 0) // the word was not on the beginning of the line
					{
						ldata.len = lastlen;
						ldata.stringwidth = lastwidth;
						// We drop the last character before this word
						ldata.nbspace--;
						string = lastword;
					}
					else // first word of the line : we have to break it
					{
						if (*string != ' ') // the character length was already added : we subtract it
						{
							const CharacterTextureCoordinates *tctc = &coords[(unsigned char)*string];
							ldata.stringwidth -= (tctc->uprewidth + tctc->uwidth + tctc->upostwidth) * width + hlead;
							if (lastword == string)
								string++;
							lastword = string;
						}
					}
					lastcharacterwasspace = TRUE;
					break;
				}
			}

			ldata.len++;
			string++;
		}

		if (paragraphstart)
		{
			ldata.len = -ldata.len;
			paragraphstart = FALSE;
		}

		if (*string == '\n')
		{
			paragraphstart = TRUE;
			// Patch for infinite loop
			if (*lastword == '\n')
				string++;
		}
		// end of a line (or text)
		if (ldata.stringwidth > textwidth)
			textwidth = ldata.stringwidth;
		if ((ldata.len < 0) && (linecount != 0))
			textheight += paragraphIndentY;
		textheight += verticalspace;
		linecount++;

		font->m_FontManager->AddLine(ldata);

		if (*string && string != lastword && *lastword != '\n')
			string++;
	}

	// now we have to draw the strings
	VxVector pos(textZone.left, textZone.top, 0.0f);

	font->m_LineCount = linecount;

	///
	// Vertical Alignment Calculation
	if (alignTop) // Top
	{
		pos.y = textZone.top;
	}
	else if (alignBottom) // Bottom
	{
		pos.y = textZone.bottom - textheight;
	}
	else // Center
	{
		pos.y = (textZone.bottom + textZone.top) * 0.5f - textheight * 0.5f;
	}

	///
	// Horizontal Alignment Calculation
	if (alignLeft) // Top
	{
		pos.x = textZone.left;
	}
	else if (alignRight) // Bottom
	{
		pos.x = textZone.right - textwidth;
	}
	else // Center
	{
		pos.x = (textZone.right + textZone.left) * 0.5f - textwidth * 0.5f;
	}

	font->m_TextExtents.SetDimension(pos.x, pos.y, textwidth, textheight);

	VxRect backgroundRect = font->m_TextExtents;
	if (options & TEXT_JUSTIFIED)
	{
		backgroundRect.left = textZone.left;
		backgroundRect.right = textZone.right;
	}

	///
	// Resize
	if (obj) // Entity to resize ?
	{
		if (options & (TEXT_RESIZE_VERT | TEXT_RESIZE_HORI))
		{
			drawzone.top = pos.y;
			if (options & TEXT_JUSTIFIED) // Only vertically
			{
				drawzone.bottom = drawzone.top + textheight;
			}
			else
			{
				if (options & TEXT_RESIZE_HORI)
					drawzone.right = drawzone.left + textwidth;
				if (options & TEXT_RESIZE_VERT)
					drawzone.bottom = drawzone.top + textheight;
			}

			Vx2DVector v;
			((CK2dEntity *)obj)->GetSize(v);
			if (options & TEXT_RESIZE_HORI)
				v.x = drawzone.right - drawzone.left;
			if (options & (TEXT_RESIZE_VERT | TEXT_JUSTIFIED))
				v.y = drawzone.bottom - drawzone.top;
			((CK2dEntity *)obj)->SetSize(v);
		}
	}

	///
	// Scroll Vertical
	if (alignTop || alignBottom) // Top|Bottom
	{
		pos.y += font->m_Offset.y;
	}

	VxRect clipRect = textZone;
	if (options & TEXT_SCREENCLIP)
	{
		if (!clipRect.Clip(font->m_ClippingRect))
			return FALSE;
	}
	CKBOOL useClip = (options & TEXT_CLIP) != 0;

	///
	// Background
	if (options & TEXT_BACKGROUND)
	{
		TextDrawCmd rectCmd;
		rectCmd.type = TextDrawCmd::CMD_RECT;
		rectCmd.font = font;
		rectCmd.material = td->m_Mat;
		rectCmd.options = options;
		rectCmd.clip = VxRect(0.0f, 0.0f, 0.0f, 0.0f);
		rectCmd.useClip = FALSE;
		rectCmd.indexStart = 0;
		rectCmd.indexCount = 0;
		rectCmd.rect = backgroundRect;
		rectCmd.lighted = (font->m_Properties & FONT_LIGHTING) != 0;
		rectCmd.transform = FALSE;
		rectCmd.geom = NULL;
		rectCmd.caretX = 0.0f;
		rectCmd.caretY = 0.0f;
		rectCmd.caretW = 0.0f;
		rectCmd.caretH = 0.0f;
		rectCmd.caretFlags = 0;
		rectCmd.caretMaterial = NULL;
		rectCmd.caretSize = 0.0f;
		batch.cmds.PushBack(rectCmd);
	}

	TextEmitContext emitCtx;
	emitCtx.batch = &batch;
	emitCtx.font = font;
	emitCtx.options = options;
	emitCtx.clip = clipRect;
	emitCtx.useClip = useClip;

	TextDrawEmitter emitter;
	emitter.EmitCaret = EmitCaretCommand;
	emitter.user = &emitCtx;

	// The actual Drawing
	for (int i = 0; i < linecount; ++i)
	{
		// current line
		LineData *data = font->m_FontManager->GetLine(i);

		font->m_SpaceSize = spacesize;
		font->m_HLeading = hlead;
		// We update the line width
		font->m_LineWidth = data->stringwidth;

		///
		// Horizontal Alignment Calculation and spacing
		if (options & TEXT_JUSTIFIED)
		{
			if (data->nbspace) // there is space, so we work on them
			{
				float delta = data->stringwidth - zonewidth;
				if (delta > 0)
				{
					font->m_SpaceSize = spacesize - delta / data->nbspace;
				}
				else
				{
					font->m_SpaceSize = spacesize + -delta / data->nbspace;
				}
			}
			else // no space, we adjust leading
			{
				float delta = zonewidth - data->stringwidth;
				int dlen = data->len;
				if (dlen < 0)
					dlen = -dlen;
				if (dlen > 1)
				{
					if (delta > 0)
					{
						font->m_HLeading = hlead + delta / (dlen - 1);
					}
					else
					{
						font->m_HLeading = hlead - -delta / (dlen - 1);
					}
				}
			}
			// the text has to be on the extreme left side
			pos.x = textZone.left;
		}
		else // The text is not justified, so we have to calculate where it should be horizontally
		{
			if (alignLeft) // Left
			{
				pos.x = textZone.left + font->m_Offset.x;
			}
			else if (alignRight) // Right
			{
				pos.x = font->m_Offset.x + textZone.right - data->stringwidth;
			}
			else // Center
			{
				pos.x = (textZone.right + textZone.left) * 0.5f - data->stringwidth * 0.5f;
			}
		}

		// Paragraph Indentation
		if (data->len < 0)
		{
			data->len = -data->len;
			if (alignLeft)
			{
				pos.x += paragraphIndentX;
			}
			if (i != 0)
				pos.y += paragraphIndentY;
		}

		if (data->len > 0)
		{
			CKDWORD buildOptions = options;
			if (useClip)
				buildOptions |= TEXT_SCISSOR;

			int indexStart = geomData->GetIndexCount();
			if (font->m_Properties & FONT_SHADOW)
				AppendStringShadowedBatch(font, dev, data->string, data->len, pos, textZone, buildOptions, geomData, &emitter);
			else
				font->AppendStringGeometry(dev, data->string, data->len, pos, textZone, buildOptions, geomData, &emitter);

			int indexCount = geomData->GetIndexCount() - indexStart;
			if (indexCount > 0)
			{
				TextDrawCmd textCmd;
				textCmd.type = TextDrawCmd::CMD_TEXT;
				textCmd.font = font;
				textCmd.material = NULL;
				textCmd.options = options;
				textCmd.clip = clipRect;
				textCmd.useClip = useClip;
				textCmd.indexStart = indexStart;
				textCmd.indexCount = indexCount;
				textCmd.rect = VxRect(0.0f, 0.0f, 0.0f, 0.0f);
				textCmd.lighted = FALSE;
				textCmd.transform = FALSE;
				textCmd.geom = targetData;
				textCmd.caretX = 0.0f;
				textCmd.caretY = 0.0f;
				textCmd.caretW = 0.0f;
				textCmd.caretH = 0.0f;
				textCmd.caretFlags = 0;
				textCmd.caretMaterial = NULL;
				textCmd.caretSize = 0.0f;

				PushTextCommand(batch, textCmd);
			}
		}

		pos.y += verticalspace;
	}

	if (targetData)
		targetData->m_DrawZone = backgroundRect;

	return TRUE;
}

/*************************************************
Name: DrawTextCallback

Summary: Retrieve the text data stored by text blocks with
				multiple setting checked and send it to render functions

Arguments:	dev : current render context
				 arg : a pointer to the CKFontManager

Return Value: none

Remarks: arg must not be NULL and point to the good manager !

See also:

*************************************************/
void CKFontManager::DrawTextCallback2D(CKRenderContext *dev, void *arg)
{
	CKFontManager *fm = (CKFontManager *)arg;

	CKBeObject *obj;
    TextData *td;

	int i, limit = fm->m_2DTexts.Size();
	fm->SortTextList2D();

	// Draw pending 2D texts
	TextDrawBatch &batch = *fm->m_TextDrawBatch2D;
	batch.Reset();
	fm->m_TextBatchCmds2D = 0;
	fm->m_TextBatchTextCmds2D = 0;
	fm->m_TextBatchCaretCmds2D = 0;
	fm->m_TextBatchRectCmds2D = 0;
	fm->m_TextBatchIndices2D = 0;

	for (i = 0; i < limit; i++)
	{
		td = fm->m_2DTexts[i];
		obj = (CKBeObject *)fm->m_Context->GetObject(td->m_Entity);
		if (obj)
		{
			// If the entity is hidden don't draw
			if (parent2DHidden((CK2dEntity *)obj))
				continue;
			// Restore entity parameters in texture font
			CKTextureFont *font = fm->GetFont(td->m_FontIndex);
			if (!font)
				continue;

			font->m_Scale = td->m_Scale;
			font->m_StartColor = td->m_StartColor;
			font->m_EndColor = td->m_EndColor;

			font->m_Margins = td->m_Margins;
			font->m_Offset = td->m_Offset;
			font->m_ParagraphIndentation = td->m_ParagraphIndentation;
			font->m_CaretMaterial = td->m_CaretMaterial;
			font->m_CaretSize = td->m_CaretSize;
			font->m_ClippingRect = td->m_ClippingRect;

			const CKBOOL wantsCompiled = (td->m_TextFlags & TEXT_COMPILED) != 0;
			const CKBOOL canCompile = (td->m_TextFlags & TEXT_SHOWCARET) == 0;
			if (canCompile)
			{
				CompiledTextData *ctdata = fm->GetCompiledText(td->m_Entity);
				CKBOOL useCached = FALSE;
				CKBOOL built = FALSE;

				if (wantsCompiled)
				{
					if (!ctdata)
						ctdata = fm->AddCompiledText(td->m_Entity);
					if (td->m_AutoCacheValid && ctdata)
					{
						useCached = TRUE;
					}
					else
					{
						CKBOOL builtNow = BuildTextBatch2D(dev, obj, td, font, batch, ctdata);
						td->m_AutoCacheValid = builtNow;
						built = TRUE;
					}
				}
				else
				{
					CKDWORD key = BuildTextCacheKey(font, td, dev);
					CKBOOL cacheMatch = (td->m_AutoCacheValid && td->m_AutoCacheKey == key && ctdata);
					if (cacheMatch)
					{
						useCached = TRUE;
					}
					else if (td->m_AutoCacheKey == key)
					{
						if (!ctdata)
							ctdata = fm->AddCompiledText(td->m_Entity);
						CKBOOL builtNow = BuildTextBatch2D(dev, obj, td, font, batch, ctdata);
						if (builtNow)
						{
							td->m_AutoCacheValid = TRUE;
						}
						else
						{
							td->m_AutoCacheValid = FALSE;
						}
						built = TRUE;
					}
					else
					{
						td->m_AutoCacheKey = key;
						td->m_AutoCacheValid = FALSE;
						BuildTextBatch2D(dev, obj, td, font, batch, NULL);
						built = TRUE;
					}
				}

				if (useCached && ctdata)
				{
					VxRect textZone = td->m_TextZone;
					textZone.left += font->m_Margins.left;
					textZone.right -= font->m_Margins.right;
					textZone.top += font->m_Margins.top;
					textZone.bottom -= font->m_Margins.bottom;

					VxRect clipRect = textZone;
					if (td->m_TextFlags & TEXT_SCREENCLIP)
					{
						if (!clipRect.Clip(font->m_ClippingRect))
							continue;
					}
					CKBOOL useClip = (td->m_TextFlags & TEXT_CLIP) != 0;

					if (td->m_TextFlags & TEXT_BACKGROUND)
					{
						TextDrawCmd rectCmd;
						rectCmd.type = TextDrawCmd::CMD_RECT;
						rectCmd.font = font;
						rectCmd.material = td->m_Mat;
						rectCmd.options = td->m_TextFlags;
						rectCmd.clip = VxRect(0.0f, 0.0f, 0.0f, 0.0f);
						rectCmd.useClip = FALSE;
						rectCmd.indexStart = 0;
						rectCmd.indexCount = 0;
						rectCmd.rect = ctdata->m_DrawZone;
						rectCmd.lighted = (font->m_Properties & FONT_LIGHTING) != 0;
						rectCmd.transform = FALSE;
						rectCmd.geom = NULL;
						rectCmd.caretX = 0.0f;
						rectCmd.caretY = 0.0f;
						rectCmd.caretW = 0.0f;
						rectCmd.caretH = 0.0f;
						rectCmd.caretFlags = 0;
						rectCmd.caretMaterial = NULL;
						rectCmd.caretSize = 0.0f;
						batch.cmds.PushBack(rectCmd);
					}

					if (ctdata->GetIndexCount() > 0)
					{
						TextDrawCmd textCmd;
						textCmd.type = TextDrawCmd::CMD_TEXT;
						textCmd.font = font;
						textCmd.material = NULL;
						textCmd.options = td->m_TextFlags;
						textCmd.clip = clipRect;
						textCmd.useClip = useClip;
						textCmd.indexStart = 0;
						textCmd.indexCount = ctdata->GetIndexCount();
						textCmd.rect = VxRect(0.0f, 0.0f, 0.0f, 0.0f);
						textCmd.lighted = FALSE;
						textCmd.transform = FALSE;
						textCmd.geom = ctdata;
						textCmd.caretX = 0.0f;
						textCmd.caretY = 0.0f;
						textCmd.caretW = 0.0f;
						textCmd.caretH = 0.0f;
						textCmd.caretFlags = 0;
						textCmd.caretMaterial = NULL;
						textCmd.caretSize = 0.0f;

						PushTextCommand(batch, textCmd);
					}
				}
				else if (!built)
				{
					BuildTextBatch2D(dev, obj, td, font, batch, NULL);
				}
			}
			else
			{
				BuildTextBatch2D(dev, obj, td, font, batch, NULL);
			}
		}
	}

	fm->m_TextBatchCmds2D = batch.cmds.Size();
	fm->m_TextBatchIndices2D = 0;
	for (i = 0; i < batch.cmds.Size(); ++i)
	{
		switch (batch.cmds[i].type)
		{
		case TextDrawCmd::CMD_TEXT:
			fm->m_TextBatchTextCmds2D++;
			fm->m_TextBatchIndices2D += batch.cmds[i].indexCount;
			break;
		case TextDrawCmd::CMD_CARET:
			fm->m_TextBatchCaretCmds2D++;
			break;
		case TextDrawCmd::CMD_RECT:
			fm->m_TextBatchRectCmds2D++;
			break;
		}
	}

	if (!batch.cmds.Size())
		return;

	VxRect oldclip;
	dev->GetViewRect(oldclip);
	VxRect currentClip = oldclip;
	CKBOOL clipActive = FALSE;
	CKTextureFont *lastFont = NULL;
	CKDWORD lastOptions = 0;
	CKBOOL lastStateValid = FALSE;

	dev->SetState(VXRENDERSTATE_ALPHABLENDENABLE, TRUE);
	dev->SetState(VXRENDERSTATE_ZWRITEENABLE, FALSE);

	for (i = 0; i < batch.cmds.Size(); ++i)
	{
		TextDrawCmd &cmd = batch.cmds[i];
		const CKBOOL needsClip = cmd.useClip && (cmd.type == TextDrawCmd::CMD_TEXT || cmd.type == TextDrawCmd::CMD_CARET);

		if (needsClip)
		{
			VxRect clip = oldclip;
			if (!clip.Clip(cmd.clip))
				continue;

			if (!clipActive || !RectEquals(clip, currentClip))
			{
				dev->SetViewRect(clip);
				currentClip = clip;
				clipActive = TRUE;
			}
		}
		else if (clipActive)
		{
			dev->SetViewRect(oldclip);
			currentClip = oldclip;
			clipActive = FALSE;
		}

		switch (cmd.type)
		{
		case TextDrawCmd::CMD_RECT:
			DrawFillRectangle(dev, cmd.material, cmd.rect, cmd.lighted, cmd.transform);
			lastFont = NULL;
			lastStateValid = FALSE;
			break;
		case TextDrawCmd::CMD_TEXT:
			if (!lastStateValid || lastFont != cmd.font || lastOptions != cmd.options)
			{
				cmd.font->SetRenderStates(dev, cmd.options);
				lastFont = cmd.font;
				lastOptions = cmd.options;
				lastStateValid = TRUE;
			}
			{
				CompiledTextData *geom = cmd.geom ? cmd.geom : &batch.data;
				const CKWORD *indices = geom->GetIndexPtr() + cmd.indexStart;
				dev->DrawPrimitive(VX_TRIANGLELIST, (CKWORD *)indices, cmd.indexCount, geom->GetDrawPrimitiveData());
			}
			break;
		case TextDrawCmd::CMD_CARET:
			if (!lastStateValid || lastFont != cmd.font || lastOptions != cmd.options)
			{
				cmd.font->SetRenderStates(dev, cmd.options);
				lastFont = cmd.font;
				lastOptions = cmd.options;
				lastStateValid = TRUE;
			}
			{
				CKMaterial *oldMat = cmd.font->m_CaretMaterial;
				float oldSize = cmd.font->m_CaretSize;
				cmd.font->m_CaretMaterial = cmd.caretMaterial;
				cmd.font->m_CaretSize = cmd.caretSize;
				cmd.font->DrawCaret(dev, cmd.caretX, cmd.caretY, cmd.caretW, cmd.caretH, cmd.caretFlags);
				cmd.font->m_CaretMaterial = oldMat;
				cmd.font->m_CaretSize = oldSize;
			}
			lastStateValid = FALSE;
			break;
		}
	}

	if (clipActive)
		dev->SetViewRect(oldclip);
}

void CKFontManager::DrawTextCallback3D(CKRenderContext *dev, void *arg)
{
	CKFontManager *fm = (CKFontManager *)arg;

	CKBeObject *obj;
    TextData *td;
	CK3dEntity *ent3;

	int limit = fm->m_3DTexts.Size();
	int i;
	fm->SortTextList3D(dev);

	// Draw pending 3D texts
	for (i = 0; i < limit; i++)
	{
		td = fm->m_3DTexts[i];
		obj = (CKBeObject *)fm->m_Context->GetObject(td->m_Entity);
		if (obj)
		{
			ent3 = (CK3dEntity *)obj;
			if (!ent3->IsAllOutsideFrustrum())
			{
				// If the entity is hidden don't draw the text
				if (parent3DHidden(ent3))
					continue;

				///
				// We set the transformation to the 3D Entity (slightly transformed)
				const VxMatrix &mat = ent3->GetWorldMatrix();
				// we invert the Y of the matrix, so the text is written in a good way
				VxVector entScale;
				ent3->GetScale(&entScale, FALSE);
				VxMatrix resMat = mat;
                resMat[0] = (mat[0] / entScale.x);
                resMat[1] = -(mat[1] / entScale.y);
                resMat[2] = -(mat[2] / entScale.z);
				dev->SetWorldTransformationMatrix(resMat);

				// Restore the entity parameter in the font texture
				CKTextureFont *font = fm->GetFont(td->m_FontIndex);
				if (!font)
					continue;
				font->m_Scale = td->m_Scale;
				font->m_StartColor = td->m_StartColor;
				font->m_EndColor = td->m_EndColor;

				font->m_Margins = td->m_Margins;
				font->m_Offset = td->m_Offset;
				font->m_ParagraphIndentation = td->m_ParagraphIndentation;
				font->m_CaretMaterial = td->m_CaretMaterial;
				font->m_CaretSize = td->m_CaretSize;
				font->m_ClippingRect = td->m_ClippingRect;
				// Draw the text
				font->DrawCKText(dev, obj, td->m_String, td->m_Align, td->m_TextZone, td->m_Mat, td->m_TextFlags, TRUE);

				// Set the bounding box for clipping
				VxBbox bbox;
				bbox.Min.Set(font->m_TextExtents.left, -font->m_TextExtents.bottom, -0.01f);
				bbox.Max.Set(font->m_TextExtents.right, -font->m_TextExtents.top, 0.01f);
				// ent3->SetBoundingBox(&bbox,TRUE);
			}
		}
	}
}

/***********************************************************************/
/***********************************************************************/
//                 System font functions
/***********************************************************************/
/***********************************************************************/

/*************************************************
Name: DeleteFont

Summary: Delete a specific system font from the manager

Arguments: the font name

Return Value:

Remarks: Release the GDI handle associated with this font
Delete unused fonts free up system resources

See also:

*************************************************/
void CKFontManager::DeleteFontHandle(CKSTRING fontName)
{
#ifndef FONTMANAGER_NOSYSFONT
	// We localize the font
	FontIterator it = m_Fonts.Find(XString(fontName));
	// And we release the handle
	if (it != m_Fonts.End())
	{
#if defined(_WIN32) || defined(WIN32)
		::DeleteObject(*it);
#else
		if (m_SelectedFont == *it)
			m_SelectedFont = NULL;
		delete static_cast<StbFontHandle *>(*it);
#endif
		// Remove entry
		m_Fonts.Remove(it);
	}
#endif
}

/*************************************************
Name: CreateFont

Summary: Create a new logical font having its name and properties

Arguments: fontName - the name used to retrieve the font in the manager
				  sysFontIndex - a system font index from enumeration  (CKPGUID_FONTNAME)
				  weight - the weight of the font (a value between 100 and 900 (step by 100)) use the enumeration
							   'Font Weight' CKPGUID_FONTWEIGHT
				  italic - TRUE if we want an italic font
				  underline - TRUE if we want an underlined font
				  resolution - determine the size of the font (CKPGUID_FONTRESOLUTION enumeration)
									use 1 --> 128x128 texture
										 2 --> 256x256
										 4 --> 512x512
										 5 --> 1024x1024
										other value --> 512x512

Return Value: TRUE if success

Remarks:

See also:

*************************************************/

CKBOOL CKFontManager::CreateFont(CKSTRING fontName, int sysFontIndex, int weight, CKBOOL italic, CKBOOL underline, int resolution, int iFontSize)
{
#ifndef FONTMANAGER_NOSYSFONT
	// No font name : do nothing
	if (!fontName)
		return FALSE;

	CKParameterManager *pm = m_Context->GetParameterManager();
	if (!pm)
		return FALSE;
	CKEnumStruct *data = pm->GetEnumDescByType(pm->ParameterGuidToType(CKPGUID_FONTNAME));
	if ((sysFontIndex < 0) || (sysFontIndex >= data->NbData))
		return FALSE;

	// We check if the fontName is present
	FontIterator it = m_Fonts.Find(XString(fontName));
	if (it == m_Fonts.End()) // We create it
	{
		// We select the font size according to the desired texture resolution
		int fontSize = FONTSIZE512;
		if (!iFontSize)
		{
			switch (resolution)
			{
			case 1: // 128x128
				fontSize = FONTSIZE128;
				break;
			case 2: // 256x256
				fontSize = FONTSIZE256;
				break;
			case 4: // 512x512
				fontSize = FONTSIZE512;
				break;
			case 8: // 1024x1024
				fontSize = FONTSIZE1024;
				break;
			default:
				break;
			}
		}
		else
		{
			fontSize = iFontSize;
		}

		// We create the logical font and retrieves a font handle
#if defined(_WIN32) || defined(WIN32)
		FONTHANDLE fontHandle = (FONTHANDLE)VxCreateFont(data->Desc[sysFontIndex], fontSize, weight, italic, underline);
#else
		CKStbSystemFontFace face;
		XClassArray<CKStbSystemFontFace> faces;
		CKStbEnumerateSystemFonts(faces);
		if (!CKStbFindBestFontFace(faces, data->Desc[sysFontIndex], weight, italic, face))
			return FALSE;
		FONTHANDLE fontHandle = CreateStbFontHandle(face, AbsInt(fontSize), weight, italic, underline);
#endif

		// If the font can't be created
		if (fontHandle == NULL)
			return FALSE;

		// We add the new font to the list
		m_Fonts.Insert(XString(fontName), fontHandle);
	}

	return TRUE;
#else
	return FALSE;
#endif
}

/*************************************************
Name: GetOutlineFontMetric

Summary: Retrieve the outline font metric for the currently selected

Arguments: The name of the font in the manager and a FONT_OUTLINEMETRIC struct
					or a FONT_METRIC struct to fill

Return Value: TRUE if success

Remarks: See the Win32 documentation for the content of OUTLINETEXTMETRIC or TEXTMETRIC
				structs

See also:

*************************************************/
CKBOOL CKFontManager::GetOutlineFontMetric(FONT_OUTLINEMETRIC &metric)
{
#ifndef FONTMANAGER_NOSYSFONT
#if defined(_WIN32) || defined(WIN32)
	return ::GetOutlineTextMetrics(m_DC, sizeof(FONT_OUTLINEMETRIC), &metric) != 0;
#else
	(void)metric;
	return m_SelectedFont != NULL;
#endif
#else
	return FALSE;
#endif
}

CKBOOL CKFontManager::GetFontMetrics(FONT_METRIC &metric)
{
#ifndef FONTMANAGER_NOSYSFONT
#if defined(_WIN32) || defined(WIN32)
	return ::GetTextMetrics(m_DC, &metric);
#else
	if (!m_SelectedFont)
		return FALSE;
	metric = static_cast<StbFontHandle *>(m_SelectedFont)->metrics;
	return TRUE;
#endif
#else
	return FALSE;
#endif
}

/*************************************************
Name: IsTrueTypeFont

Summary: test if the currently selected font is a true type font

Arguments: none

Return Value: TRUE if it's a true type

Remarks:

See also:

*************************************************/
CKBOOL CKFontManager::IsTrueTypeFont()
{
#ifndef FONTMANAGER_NOSYSFONT
#if defined(_WIN32) || defined(WIN32)
	TEXTMETRIC metric;
	if (::GetTextMetrics(m_DC, &metric))
		return (metric.tmPitchAndFamily & TMPF_TRUETYPE);
	return FALSE;
#else
	return m_SelectedFont != NULL;
#endif
#else
	return FALSE;
#endif
}

/*************************************************
Name: SelectFont

Summary: Select the current font

Arguments: fontName - the font name in the manager

Return Value: TRUE if success

Remarks: must be call before use of get*fontMetrics, getchar*width

See also:

*************************************************/
CKBOOL CKFontManager::SelectFont(CKSTRING fontName)
{
#ifndef FONTMANAGER_NOSYSFONT
	// We try to localize the font
	FontIterator it = m_Fonts.Find(XString(fontName));
	if (it == m_Fonts.End())
		return FALSE;

	// If it is present, we select it in the DC and retrieves the information
#if defined(_WIN32) || defined(WIN32)
	return (::SelectObject(m_DC, *it) != NULL);
#else
	m_SelectedFont = *it;
	return TRUE;
#endif
#else
	return FALSE;
#endif
}

/*************************************************
Name: GetCharABCWidth

Summary: Retrieves the width of a range of char for the current selected font

Arguments:  startChar/endChar - define the character range to use (inclusive range)
				  ABCWidth - an array of FONT_ABC structs to store the results

Return Value: TRUE if success

Remarks: the FONT_ABC array must be large enough to store the result

See also:

*************************************************/
CKBOOL CKFontManager::GetCharABCWidths(CKBYTE startChar, CKBYTE endChar, FONT_ABC *ABCWidths)
{
#ifndef FONTMANAGER_NOSYSFONT
#if defined(_WIN32) || defined(WIN32)
	return ::GetCharABCWidths(m_DC, startChar, endChar, ABCWidths);
#else
	if (!m_SelectedFont || !ABCWidths || endChar < startChar)
		return FALSE;
	StbFontHandle *handle = static_cast<StbFontHandle *>(m_SelectedFont);
	for (int i = startChar; i <= endChar; ++i)
		ABCWidths[i - startChar] = handle->abc[i];
	return TRUE;
#endif
#else
	return FALSE;
#endif
}

/*************************************************
Name: GetCharWidth

Summary: Retrieves the width of a range of char for non true-type font currently selected

Arguments:  startChar/endChar - define the character range to use (inclusive range)
				  Widths - an array of int to store the results

Return Value: TRUE if success

Remarks: the widths array must be large enough to store the result

See also:

*************************************************/
CKBOOL CKFontManager::GetCharWidths(CKBYTE startChar, CKBYTE endChar, int *widths)
{
#ifndef FONTMANAGER_NOSYSFONT
#if defined(_WIN32) || defined(WIN32)
	return ::GetCharWidth32(m_DC, startChar, endChar, widths);
#else
	if (!m_SelectedFont || !widths || endChar < startChar)
		return FALSE;
	StbFontHandle *handle = static_cast<StbFontHandle *>(m_SelectedFont);
	for (int i = startChar; i <= endChar; ++i)
		widths[i - startChar] = handle->widths[i];
	return TRUE;
#endif
#else
	return FALSE;
#endif
}

/*************************************************
Name: CreateTextureFromFont

Summary: Create a CKTexture representing the characters of the designed font

Arguments: sysFontIndex - the index of the font in enumeration CKPGUID_FONTNAME
				 resolution - the size of the texture (CKPGUID_FONTRESOLUTION enumeration)
									use 1 --> 128x128 texture
										 2 --> 256x256
										 4 --> 512x512
										 8 --> 1024x1024
										other value --> 512x512
				 extended - if false, we create a half height texture with only the first 128 ASCII chars
								  else, we have a full size texture with all the chars
				  renderControls - if false, we don't draw the chars with ASCII code < 32
											(i.e. control characters are all blank)

					dynamic - if true, the texture created is a dynamical object

Return Value: the texture if sucess NULL otherwise

Remarks:

See also:

*************************************************/
CKTexture *CKFontManager::CreateTextureFromFont(int sysFontIndex, int resolution, CKBOOL extended, int weight, CKBOOL italic, CKBOOL underline, CKBOOL renderControls, CKBOOL dynamic, int iForcedSize)
{
#ifndef FONTMANAGER_NOSYSFONT
	CKParameterManager *pm = m_Context->GetParameterManager();
	if (!pm)
		return NULL;

	CKEnumStruct *data = pm->GetEnumDescByType(pm->ParameterGuidToType(CKPGUID_FONTNAME));
	if ((sysFontIndex < 0) || (sysFontIndex >= data->NbData))
		return NULL;

	// Get the font name by index
	CKSTRING fontName = data->Desc[sysFontIndex];

	// we compose the texture name
	XString textureName("FONT_");
	textureName << fontName;
	switch (resolution)
	{
	case 1:
		textureName << "_128x128";
		break;
	case 2:
		textureName << "_256x256";
		break;
	case 4:
		textureName << "_512x512";
		break;
	case 8:
		textureName << "_1024x1024";
		break;
	}

	if (iForcedSize)
	{
		textureName << "(" << iForcedSize << ")";
	}

	if (extended)
		textureName << "_E";
	if (renderControls)
		textureName << "_C";
	textureName << "_";
	switch (weight)
	{
	case 100:
		textureName << "THIN";
		break;
	case 200:
		textureName << "XLIGHT";
		break;
	case 300:
		textureName << "LIGHT";
		break;
	case 400:
		textureName << "NORMAL";
		break;
	case 500:
		textureName << "MEDIUM";
		break;
	case 600:
		textureName << "DBOLD";
		break;
	case 700:
		textureName << "BOLD";
		break;
	case 800:
		textureName << "XBOLD";
		break;
	case 900:
		textureName << "HEAVY";
		break;
	}

	if (italic)
		textureName << "_I";
	if (underline)
		textureName << "_U";

	CKTexture *texture = (CKTexture *)m_Context->GetObjectByNameAndClass(textureName.Str(), CKCID_TEXTURE);
	if (!texture)
	{
		// We try to localize the font
		FontIterator it = m_Fonts.Find(textureName);
		if (it == m_Fonts.End())
		{
			if (!CreateFont(textureName.Str(), sysFontIndex, weight, italic, underline, resolution, iForcedSize))
				return NULL;
			// exists this time because we just create it !
			it = m_Fonts.Find(textureName);
		}

		// CKTexture object creation
		if (dynamic)
			texture = (CKTexture *)m_Context->CreateObject(CKCID_TEXTURE, textureName.Str(), CK_OBJECTCREATION_DYNAMIC);
		else
			texture = (CKTexture *)m_Context->CreateObject(CKCID_TEXTURE, textureName.Str());
		// If we couldn't create it
		if (!texture)
			return NULL;

		int nbLine = 8, nbCol = 16;
		int texHeight = 64, texWidth = 128;

		// If we draw all 256 chars
		if (extended)
		{
			nbLine = 16;
			texHeight *= 2;
		}

		// Texture size from desired resolution
		texWidth *= resolution;
		texHeight *= resolution;

		int nbPixels = texHeight * texWidth, charCellSize = texWidth / nbCol;
		// We create the surface
		texture->Create(texWidth, texHeight, 32);

		// We get the texture description
		VxImageDescEx vxTexDesc;
		texture->GetSystemTextureDesc(vxTexDesc);

		vxTexDesc.Image = texture->LockSurfacePtr();

		if (vxTexDesc.Image)
		{
			// We clear the texture surface (white for the color and black for the alpha)
			CKDWORD colorDef = RGBAITOCOLOR(0xff, 0xff, 0xff, 0x00);

			CKBYTE *AlphaDest = (CKBYTE *)vxTexDesc.Image + 3;

			VxFillStructure(nbPixels, (CKBYTE *)vxTexDesc.Image, 4, 4, &colorDef);

#if defined(_WIN32) || defined(WIN32)
			// We create a system memory bitmap for font rendering
			BITMAP_HANDLE hBitmap = VxCreateBitmap(vxTexDesc);
			// If it exists we get the handle
			HFONT hFont = *it;
			// We select the GDI objects
			HFONT oldFont = (HFONT)::SelectObject(m_DC, hFont);
			HBITMAP oldBitmap = (HBITMAP)::SelectObject(m_DC, (HBITMAP)hBitmap);

			// Set colors
			RECT r;
			SetRect(&r, 0, 0, texWidth, texHeight);
			::FillRect(m_DC, &r, (HBRUSH)::GetStockObject(BLACK_BRUSH));
			::SetTextColor(m_DC, RGBAITOCOLOR(255, 255, 255, 0));
			::SetBkColor(m_DC, RGBAITOCOLOR(0, 0, 0, 0));

			// Rendering rectangle
			RECT rect;
			rect.top = 0;
			rect.left = 0;
			rect.bottom = charCellSize;
			rect.right = charCellSize;

			// For each character, we draw it in the bitmap at the good position
			int i, j;
			char c;

			// Render a grid

			HPEN hp = ::CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
			HPEN bp = ::CreatePen(PS_SOLID, 1, RGB(0, 0, 0));

			for (i = 0; i < nbLine; i++)
			{
				for (j = 0; j < nbCol; j++)
				{
					c = i * nbCol + j;
					if (renderControls || ((unsigned char)c >= 32))
						::DrawText(m_DC, &c, 1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX);
					rect.left += charCellSize;
					rect.right += charCellSize;
				}
				rect.left = 0;
				rect.right = charCellSize;
				rect.top += charCellSize;
				rect.bottom += charCellSize;
			}

			for (i = 0; i < nbLine; i++)
			{
				(HPEN)::SelectObject(m_DC, bp);
				::MoveToEx(m_DC, i * charCellSize, 0, NULL);
				::LineTo(m_DC, i * charCellSize, texHeight);
				(HPEN)::SelectObject(m_DC, bp);
				::MoveToEx(m_DC, i * charCellSize - 1, 0, NULL);
				::LineTo(m_DC, i * charCellSize - 1, texHeight);

				(HPEN)::SelectObject(m_DC, bp);
				::MoveToEx(m_DC, 0, i * charCellSize, NULL);
				::LineTo(m_DC, texWidth, i * charCellSize);
				(HPEN)::SelectObject(m_DC, bp);
				::MoveToEx(m_DC, 0, i * charCellSize - 1, NULL);
				::LineTo(m_DC, texWidth, i * charCellSize - 1);
			}

			// For each pixel of the bitmap, we reduce to a grayscale color
			// and fill an alpha table
			BITMAP dib;
			::GetObjectA(hBitmap, sizeof(BITMAP), &dib);
			CKDWORD Bpp = dib.bmBitsPixel >> 3;
			CKDWORD Pitch = dib.bmWidthBytes;
			CKBYTE *Colors = (CKBYTE *)dib.bmBits + (dib.bmHeight - 1) * dib.bmWidthBytes;

			CKDWORD RGBIntensity[3][256];
			for (i = 0; i < 256; ++i)
			{
				RGBIntensity[0][i] = (unsigned long)(0.299f * (float)i);
				RGBIntensity[1][i] = (unsigned long)(0.587f * (float)i);
				RGBIntensity[2][i] = (unsigned long)(0.114f * (float)i);
			}
			if (!Colors)
			{
				for (i = 0; i < texHeight; i++)
					for (j = 0; j < texWidth; j++, AlphaDest += 4)
					{
						COLORREF currentColor = ::GetPixel(m_DC, j, i);
						*AlphaDest = 0xFF & (unsigned char)(RGBIntensity[0][GetRValue(currentColor)] + RGBIntensity[1][GetGValue(currentColor)] + RGBIntensity[2][GetBValue(currentColor)]);
						// ENDIANSWAP32(*AlphaDest);
					}
			}
			else
			{
				for (i = 0; i < texHeight; i++, Colors -= Pitch)
				{
					CKBYTE *col = Colors;
					for (j = 0; j < texWidth; j++, AlphaDest += 4, col += Bpp)
					{
						*AlphaDest = 0xff & (unsigned char)(RGBIntensity[2][col[0]] + RGBIntensity[1][col[1]] + RGBIntensity[0][col[2]]);
					}
				}
			}

			// We deselect and delete the GDI objects
			::SelectObject(m_DC, oldBitmap);
			::SelectObject(m_DC, oldFont);
			VxDeleteBitmap(hBitmap);
#else
			RenderStbFontAtlas(static_cast<StbFontHandle *>(*it), vxTexDesc, nbLine, nbCol, charCellSize, renderControls);
#endif

			texture->ReleaseSurfacePtr();
		}
		// Some texture settings
		texture->SetUserMipMapMode(TRUE);
		texture->UseMipmap(-1);

		// Wie add the new texture to the current level
		m_Context->GetCurrentLevel()->AddObject(texture);
	}

	return texture;
#else
	return NULL;
#endif
}

/*************************************************
Name: FontEnumeratorCallBack

Summary: Add a new font name to the array font name list

Arguments: font info and the array to fill (in the param field)

Return Value: TRUE in any way

Remarks: Called automatically by the system

See also: RegenerateFontEnumeration

*************************************************/
#if (defined(_WIN32) || defined(WIN32)) && !defined(FONTMANAGER_NOSYSFONT)
CKBOOL CALLBACK FontEnumeratorCallBack(LPLOGFONT lplf, LPNEWTEXTMETRIC lpntm, DWORD FontType, LPVOID param)
{
	// Retrieves the parameters
	XClassArray<XString> *array = (XClassArray<XString> *)param;
	array->PushBack(XString(lplf->lfFaceName));

	return TRUE;
}
#endif

/*************************************************
Name: RegenerateFontEnumeration

Summary: Fill an enumeration CKPGUID_FONTNAME with the list of system font names found

Arguments: -

Return Value: FALSE if fails, in this case the enum is set to empty

See also: -

*************************************************/
CKBOOL CKFontManager::RegenerateFontEnumeration()
{
#ifndef FONTMANAGER_NOSYSFONT
	CKParameterManager *pm = m_Context->GetParameterManager();
	if (!pm)
		return FALSE;

#if defined(_WIN32) || defined(WIN32)
    LOGFONT lf;
    memset(&lf, 0, sizeof(LOGFONT));
    lf.lfFaceName[0] = '\0';
    lf.lfCharSet = ANSI_CHARSET;

    XClassArray<XString> array;
    array.Reserve(64);

    // Fill the array (automatic call to FontEnumeratorCallback for each font found)
    if (!EnumFontFamiliesEx(m_DC, &lf, (FONTENUMPROC)FontEnumeratorCallBack, (LPARAM)&array, 0))
	{
		pm->ChangeEnumDeclaration(CKPGUID_FONTNAME, "");
		return FALSE;
	}
	// Presents font in lexical order
	array.Sort();

	// Generate the new enum
	XString newEnum;
	char buffer[64];
	int i = 0;
	for (XString *it = array.Begin(); it != array.End(); it++)
	{
		if (i++ != 0)
			newEnum << ",";
		newEnum << (*it);
		snprintf(buffer, sizeof(buffer), "=%d", i - 1);
		newEnum << buffer;
	}
	pm->ChangeEnumDeclaration(CKPGUID_FONTNAME, newEnum.Str());

	return TRUE;
#else
	XClassArray<CKStbSystemFontFace> faces;
	CKStbEnumerateSystemFonts(faces);
	if (faces.Size() == 0)
	{
		pm->ChangeEnumDeclaration(CKPGUID_FONTNAME, "");
		return FALSE;
	}

	XClassArray<XString> seen;
	XClassArray<XString> array;
	array.Reserve(faces.Size());
	for (int faceIndex = 0; faceIndex < faces.Size(); ++faceIndex)
	{
		XString normalized = faces[faceIndex].family;
		normalized.ToLower();
		if (seen.Find(normalized) == seen.End())
		{
			seen.PushBack(normalized);
			array.PushBack(faces[faceIndex].family);
		}
	}

	CKStbSystemFontFace arialFace;
	if (seen.Find(XString("arial")) == seen.End() &&
		CKStbFindBestFontFace(faces, "Arial", 400, FALSE, arialFace))
	{
		array.PushBack(XString("Arial"));
	}

	array.Sort();

	XString newEnum;
	char buffer[64];
	int i = 0;
	for (XString *it = array.Begin(); it != array.End(); it++)
	{
		if (i++ != 0)
			newEnum << ",";
		newEnum << (*it);
		snprintf(buffer, sizeof(buffer), "=%d", i - 1);
		newEnum << buffer;
	}
	pm->ChangeEnumDeclaration(CKPGUID_FONTNAME, newEnum.Str());

	return TRUE;
#endif
#else
	return FALSE;
#endif
}
