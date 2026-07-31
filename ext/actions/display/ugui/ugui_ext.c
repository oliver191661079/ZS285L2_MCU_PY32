/* -------------------------------------------------------------------------------- */
/* -- ¦ÌGUI - Generic GUI module (C)Achim Dubler, 2015                            -- */
/* -------------------------------------------------------------------------------- */
// ¦ÌGUI is a generic GUI module for embedded systems.
// This is a mem_free software that is open for education, research and commercial
// developments under license policy of following terms.
//
//  Copyright (C) 2015, Achim D?bler, all rights reserved.
//  URL: http://www.embeddedlightning.com/
//
// * The ¦ÌGUI module is a mem_free software and there is NO WARRANTY.
// * No restriction on use. You can use, modify and redistribute it for
//   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
// * Redistributions of source code must retain the above copyright notice.
//
/* -------------------------------------------------------------------------------- */
/* -- REVISION HISTORY                                                           -- */
/* -------------------------------------------------------------------------------- */
//  Mar 18, 2015  V0.3  Driver support added.
//                      Window and object support added.
//                      Touch support added.
//                      Fixed some minor bugs.
//
//  Oct 20, 2014  V0.2  Function UG_DrawRoundFrame() added.
//                      Function UG_FillRoundFrame() added.
//                      Function UG_DrawArc() added.
//                      Fixed some minor bugs.
//
//  Oct 11, 2014  V0.1  First release.
/* -------------------------------------------------------------------------------- */
#include <mem_manager.h>
#include <msg_manager.h>
#include "ugui.h"
#include "gui_api.h"
#include "resource.h"
#include "sdfs.h"
#include <transcode.h>

typedef struct
{
    UG_U32  dataPos;
    UG_U32  dataSize;

    UG_U16  numEntries;

    UG_U8   reserved [6];

} __attribute__((__packed__)) UG_FontIndexTableInfo;

typedef struct
{
    UG_U32  dataPos;
    UG_U32  dataSize;

    UG_U16  numEntries;

    UG_U8   reserved [6];

} __attribute__((__packed__)) UG_FontWidthTableInfo;

typedef struct
{
    UG_U32  dataPos;
    UG_U32  dataSize;

    UG_U16  numEntries;
    UG_U16  entrySize;

    UG_U8   offsValBits;  // offset bits
    UG_U8   sizeValBits;  // size bits

    UG_U8   reserved [2];

} __attribute__((__packed__)) UG_FontOffsTableInfo;

typedef struct
{
    UG_U32  dataPos;
    UG_U32  dataSize;

    UG_U8   reserved [8];

} __attribute__((__packed__)) UG_FontDecompContextInfo;

typedef struct
{
    UG_U32  dataPos;
    UG_U32  dataSize;

    UG_U8   reserved [8];

} __attribute__((__packed__)) UG_FontBitsDataInfo;

typedef struct
{
    char    fontFormat [3];
    UG_U8   swVersion;

    UG_U8   fontFlags;
    UG_U8   fontSize;

    UG_U8   charWidth;
    UG_U8   charHeight;
    UG_S8   offsetX;
    UG_S8   offsetY;

    UG_U8   bitsPerPixel;
    UG_U8   grayLevel;
    UG_U16  bitsSizePerChar;

    UG_U16  numChars;

    UG_FontIndexTableInfo     indexTableInfo;
    UG_FontWidthTableInfo     widthTableInfo;
    UG_FontOffsTableInfo      offsTableInfo;
    UG_FontDecompContextInfo  decompContextInfo;
    UG_FontBitsDataInfo       bitsDataInfo;
    UG_U8   data[0];
} __attribute__((__packed__)) UG_FontInfo;

typedef struct
{
    UG_U16  charIndex;
    UG_U16  startCode;

    UG_U16  codesMask [4];
    UG_U8   numCodes  [4];

} __attribute__((__packed__)) UG_FontIndexTableEntry;

typedef struct
{
    UG_U16  charIndex;
    UG_U16  numChars;
    UG_U8   monospace;
    UG_U8   charWidths [1];

} __attribute__((__packed__)) UG_FontWidthTableEntry;

typedef struct
{
    UG_U32  bitsOffs;
    UG_U16  bitsSize [8];

} __attribute__((__packed__)) UG_FontOffsTableEntry;

typedef struct
{
    UG_FontInfo*      fontInfo;
    UG_U8*            indexTable;
    UG_U8*            widthTable;
    UG_U8*            decompContext;
    UG_U8*            bitsData;
    UG_U8*            bitsBuf;
} UG_FontStruct;

/*!
 * \brief bit data stream
 */
#define CAL_BitStream(_data)							\
														\
	UG_U8 *_bsData  = (UG_U8*)(_data);			\
	UG_U32 _bsCount = 0;							\
	UG_U32 _bsHold  = 0

/*!
 * \brief seek bit datas
 */
#define CAL_SeekBits(_data, _bitPos)                   \
                                                       \
    _bsData  = (UG_U8*)(_data) + ((_bitPos) / 8);      \
                                                       \
    _bsCount = 8 - ((_bitPos) % 8);                    \
    _bsHold  = (*_bsData++) >> ((_bitPos) % 8)         \

/*!
 * \brief perfetch bit datas
 */
#define CAL_NeedBits(_n)                               \
                                                       \
    while (_bsCount < (UG_U32)(_n))                    \
    {                                                  \
        _bsHold  += (UG_S32)(*_bsData++) << _bsCount;  \
        _bsCount += 8;                                 \
    }

/*!
 * \brief read bit datas
 */
#define CAL_ReadBits(_n)                               \
                                                       \
    (_bsHold & ((1 << (_n)) - 1))

/*!
 * \brief drop bit datas
 */
#define CAL_DropBits(_n)                               \
                                                       \
	_bsHold >>= (_n);                                  \
	_bsCount -= (_n)


/*!
 * \brief data align
 */

#define CAL_Align(_v, _a)  \
    \
    (((_v) + ((_a) - 1)) / (_a) * (_a))


#ifdef USE_FONT_ZH_YAHEI_16X16
const UG_FONT FONT_ZH_YAHEI_16X16 = {NULL, 16, 16, FONT_TYPE_ZH};
#endif

extern UG_GUI* UG_GetGui(void);

UG_FontStruct *global_font_context = NULL;
/*
 * get char index
 */
static int UG_GetFontCharIndex(UG_FontStruct* font, UG_U32 ch)
{
    UG_FontStruct*  p = font;

    int  i, j, k, n;

    n = p->fontInfo->indexTableInfo.numEntries;

    for (i = 0, j = n - 1; i <= j; )
    {
        UG_FontIndexTableEntry*  entry;

        UG_U16  mask;

        k = i + (j - i) / 2;

        entry = &((UG_FontIndexTableEntry*)p->indexTable)[k];

        if (ch < entry->startCode)
        {
            j = k - 1;
            continue;
        }
        else if (ch >= (UG_U32)(entry->startCode + 64))
        {
            i = k + 1;
            continue;
        }

        i = ch - entry->startCode;

        j = i / 16;
        k = i % 16;

        mask = entry->codesMask[j];

        if (!(mask & (1 << k)))
            return -1;

        for (i = n = 0; i < k; i++)
        {
            if (mask & (1 << i))
                n++;
        }

        i = entry->charIndex + entry->numCodes[j] + n;

        return i;
    }

    return -1;
}

/*
 * get char width
 */
static int UG_GetFontCharWidth(UG_FontStruct* font, int charIndex)
{
    UG_FontStruct*  p = font;

    int  i, n;

    n = p->fontInfo->widthTableInfo.numEntries;

    for (i = 0; n > 0; n--)
    {
        UG_FontWidthTableEntry*  entry;

        entry = (UG_FontWidthTableEntry*)((UG_S8*)p->widthTable + i);

        if (charIndex >= entry->charIndex &&
            charIndex <  entry->charIndex + entry->numChars)
        {
            i = (entry->monospace) ? 0 : (charIndex - entry->charIndex);
            return entry->charWidths[i];
        }

        i += sizeof(UG_FontWidthTableEntry);

        if (!entry->monospace)
        {
            i += CAL_Align(entry->numChars - 1, 2);
        }
    }

    return -1;
}

/*!
 * \brief get char bit map
 */
bool UG_GetCharBitmap(UG_FontStruct *font, UG_U32 ch, UG_CharBitmap* charBitmap)
{
    UG_FontStruct*  p;
    UG_FontInfo*  t;

    int  charIndex;
    int  charWidth;

    if ((p = font) == NULL)
        return false;

    if ((charIndex = UG_GetFontCharIndex(p, ch)) < 0) {
		printk("0x%x UG_GetFontCharIndex failed \n",ch);
        return false;
	}

    if ((charWidth = UG_GetFontCharWidth(p, charIndex)) < 0) {
		printk("0x%x UG_GetFontCharWidth failed \n",ch);
        return false;
	}

	t = p->fontInfo;

    memset(charBitmap, 0, sizeof(UG_CharBitmap));

    charBitmap->frameRect.w  = t->charWidth;
    charBitmap->frameRect.h  = t->charHeight;

    charBitmap->offsetY      = (t->offsetY < 0) ? -(t->offsetY) : 0;
    charBitmap->charWidth    = charWidth;
    charBitmap->charHeight   = t->charHeight + charBitmap->offsetY;
    charBitmap->bitsPerPixel = t->bitsPerPixel;
    charBitmap->grayLevel    = t->grayLevel;
    charBitmap->bitsData     = (UG_U8 *)p->fontInfo + t->bitsDataInfo.dataPos + charIndex * t->bitsSizePerChar;
#if debug
	printk("ch %x , charWidth %d  charHeight %d bitsPerPixel %d \n ",ch, charBitmap->charWidth,charBitmap->charHeight, t->bitsPerPixel);
	printk("bitsData -- %p: %d \n ", charBitmap->bitsData ,t->charHeight * charBitmap->charWidth * t->bitsPerPixel / 8);
	for(int i = 0; i < 32; i++)
	{
		printk("0x%x ",charBitmap->bitsData[i]);
	}
	printk("\n");
#endif
    return true;
}


/*!
 * \brief get char width
 */
int UG_GetCharWidth(UG_FontStruct *font, UG_U32 ch)
{
    UG_FontStruct*  p;

    int  charIndex;
    int  charWidth;

    if ((p = font) == NULL)
        return 0;

    if ((charIndex = UG_GetFontCharIndex(p, ch)) < 0)
        return 0;

    if ((charWidth = UG_GetFontCharWidth(p, charIndex)) < 0)
        return 0;

    return charWidth;
}


/*!
 * \brief get char height
 */
int UG_GetCharHeight(UG_FontStruct *font)
{
    UG_FontStruct*  p;

    int  charHeight;

    if ((p = font) == NULL)
        return 0;

    charHeight = p->fontInfo->charHeight;

    if (p->fontInfo->offsetY < 0)
        charHeight += -(p->fontInfo->offsetY);

    return charHeight;
}
void* UG_GetFontContext(void)
{
	return global_font_context;
}

void* UG_ExtFontInit(const UG_FONT *font)
{

	UG_FontInfo *fontinfo = NULL;

	if ( !font ) return NULL;
	if ( font->font_type != FONT_TYPE_ZH) return NULL;

	global_font_context = mem_malloc(sizeof(UG_FontStruct));

	if (!font->p) {
		if(sd_fmap(FONT_16_FILE,(void**)&fontinfo, NULL)) {
			printk("font init failed \n");
		}
	} else {
		fontinfo =  (UG_FontInfo *)font->p;
	}

#if debug
	printk("fontinfo : %p \n",fontinfo);
	printk("fontFormat : %s  \n",fontinfo->fontFormat);
	printk("swVersion : %d  \n",fontinfo->swVersion);
	printk("fontFlags : %d  \n",fontinfo->fontFlags);
	printk("fontSize : %d  \n",fontinfo->fontSize);
	printk("charWidth : %d  \n",fontinfo->charWidth);
	printk("charHeight : %d  \n",fontinfo->charHeight);
	printk("offsetX : %d  \n",fontinfo->offsetX);
	printk("offsetY : %d  \n",fontinfo->offsetY);
	printk("bitsPerPixel : %d  \n",fontinfo->bitsPerPixel);
	printk("grayLevel : %d  \n",fontinfo->grayLevel);
	printk("bitsSizePerChar : %d  \n",fontinfo->bitsSizePerChar);
	printk("numChars : %d  \n",fontinfo->numChars);
	printk("\n \n");

	printk("indexTableInfo : \n");
	printk("dataPos : 0x%x \n", fontinfo->indexTableInfo.dataPos);
	printk("dataSize :  0x%x \n", fontinfo->indexTableInfo.dataSize);
	printk("numEntries : 0x%x \n",fontinfo->indexTableInfo.numEntries);
	printk("\n \n");

	printk("widthTableInfo : \n");
	printk("dataPos : 0x%x \n", fontinfo->widthTableInfo.dataPos);
	printk("dataSize :  0x%x \n", fontinfo->widthTableInfo.dataSize);
	printk("numEntries : 0x%x \n",fontinfo->widthTableInfo.numEntries);
	printk("\n \n");

	printk("offsTableInfo : \n");
	printk("dataPos : 0x%x \n", fontinfo->offsTableInfo.dataPos);
	printk("dataSize :  0x%x \n", fontinfo->offsTableInfo.dataSize);
	printk("numEntries : 0x%x \n",fontinfo->offsTableInfo.numEntries);
	printk("entrySize : 0x%x \n",fontinfo->offsTableInfo.entrySize);
	printk("offsValBits : 0x%x \n",fontinfo->offsTableInfo.offsValBits);
	printk("sizeValBits : 0x%x \n",fontinfo->offsTableInfo.sizeValBits);
	printk("\n \n");

	printk("decompContextInfo: \n");
	printk("dataPos : 0x%x \n", fontinfo->decompContextInfo.dataPos);
	printk("dataSize :  0x%x \n", fontinfo->decompContextInfo.dataSize);
	printk("\n \n");

	printk("bitsDataInfo :\n");
	printk("dataPos : 0x%x \n", fontinfo->bitsDataInfo.dataPos);
	printk("dataSize :  0x%x \n", fontinfo->bitsDataInfo.dataSize);
	printk("\n \n");
#endif

	global_font_context->fontInfo = fontinfo;
	global_font_context->indexTable = &fontinfo->data[0];
	global_font_context->widthTable = global_font_context->indexTable + fontinfo->indexTableInfo.dataSize;

	return global_font_context;
}

static inline UG_U16 UG_AlphaBlend_RGB_565(UG_U16 p1, UG_U16 p2, UG_U8 a2)
{
    UG_U32  v1 = (p1 | (p1 << 16)) & 0x07E0F81F;
    UG_U32  v2 = (p2 | (p2 << 16)) & 0x07E0F81F;

    UG_U32  v3 = (((v2 - v1) * (a2 >> 3) >> 5) + v1) & 0x07E0F81F;

    return (UG_U16)((v3 & 0xFFFF) | (v3 >> 16));
}


static inline UG_U16 UG_BlendPixel_RGB_565(UG_U16 p1, UG_U16 p2, UG_U8 a2)
{
    if (a2 >= 0xF8)
    {
        return p2;
    }
    else if (a2 >= 0x08)
	{
        return UG_AlphaBlend_RGB_565(p1, p2, a2);
    }
	else
	{
		return p1;
	}
}

void UG_PutString_Ext(UG_S16 x, UG_S16 y, UG_S16 xe, UG_S16 ye,const UG_U8* str, UG_U16 character_id)
{
	UG_GUI *gui = UG_GetGui();
	UG_U16 *unicode = NULL;
	UG_U16 *txt_str = NULL;
	UG_S16 len = 0;
	UG_S16 xp,yp;
	UG_U16 chr;
	UG_U16 charWidth;

	xp=x;
	yp=y;

	printk("character_id=%d\n", character_id);

	if (character_id == CHARACTER_SETS_UNICODE) {
		unicode = (UG_U16*)str;
		len = strlen(str);
	} else if (character_id == CHARACTER_SETS_GBK) {
		len = GUI_GBK_to_Unicode((UG_U8*)str, strlen(str), &unicode);
	} else {
		len = GUI_UTF8_to_Unicode((UG_U8*)str, strlen(str), &unicode);
	}
	if(len <= 0) return;

	txt_str = unicode;

	while ( *txt_str != 0 )
	{
		chr = *txt_str;
		if ( chr == '\n' )
		{
			 xp = gui->x_dim;
			 txt_str++;
			 continue;
		}

		charWidth = UG_GetCharWidth(UG_GetFontContext(), chr);

		if ( xp + charWidth > xe - 1 )
		{
			xp = x;
			yp += gui->font.char_height + gui->font.char_v_space;
		}

		if(yp + gui->font.char_height > ye - 1)
		{
			goto exit;
		}

		UG_PutChar_Ext(chr, xp, yp, gui->fore_color, gui->back_color);

		xp += charWidth + gui->font.char_h_space;
		txt_str++;
	}
exit:
	if (unicode && character_id != CHARACTER_SETS_UNICODE) {
		mem_free(unicode);
	}
}

void UG_PutChar_Ext( UG_U32 chr, UG_S16 x, UG_S16 y, UG_COLOR fc, UG_COLOR bc )
{
	UG_GUI* gui = UG_GetGui();
	UG_CharBitmap charBitmap;

	UG_GetCharBitmap(UG_GetFontContext(), chr ,&charBitmap);

	if (!UG_DrawChar(x, y, fc, bc, gui->font.char_height, charBitmap))
		return;

	UG_U16 i, j, xo, yo;
	UG_U16 bitsOffs = 0;
	UG_U16 color = 0;
	UG_U16 bitsVal = 0;

	yo = y;
	xo = x;

#if debug
	printk("chr 0x%x : xo %d yo %d char_height %d char_width %d bitsPerPixel %d \n",chr,xo, yo, gui->font.char_height,charBitmap.charWidth,charBitmap.bitsPerPixel);
#endif

	bitsOffs = 0;
	CAL_BitStream(charBitmap.bitsData);

	for(j = 0; j < gui->font.char_height; j++ )
	{
		xo = x;
		CAL_SeekBits(charBitmap.bitsData, bitsOffs);
		for( i= 0 ;i < charBitmap.charWidth ;i++ )
		{

			CAL_NeedBits(charBitmap.bitsPerPixel);

			bitsVal = CAL_ReadBits(charBitmap.bitsPerPixel);
					  CAL_DropBits(charBitmap.bitsPerPixel);

			color = UG_BlendPixel_RGB_565(bc, fc, 0xFF * bitsVal / (charBitmap.grayLevel  - 1));

			gui->pset(xo,yo,color);
			xo++;
		}
		yo++;
		bitsOffs += charBitmap.frameRect.w * charBitmap.bitsPerPixel;
	}
}

void UG_PutText_Ext(UG_TEXT* txt)
{
	UG_GUI* gui = UG_GetGui();
	UG_S16 sl;
	UG_S16 xp,yp;
	UG_S16 xs=txt->a.xs;
	UG_S16 ys=txt->a.ys;
	UG_S16 xe=txt->a.xe;
	UG_S16 ye=txt->a.ye;
	UG_U8  align=txt->align;
	UG_S16 char_width=txt->font->char_width;
	UG_S16 char_height=txt->font->char_height;
	UG_S16 char_h_space=txt->h_space;
	UG_U16 i,j,xo,yo;
	UG_U16 bt;
	UG_U16* unicode = NULL;
	UG_U16* txt_str = NULL;
	UG_CharBitmap charBitmap;
	UG_U16 bitsOffs = 0;
	UG_U16 bitsVal = 0;
	UG_U16 color = 0;

	if ( txt->font->p == NULL ) return;
	if ( txt->str == NULL ) return;
	if ( (ye - ys) < txt->font->char_height ) return;

	yp = 0;
	if ( align & (ALIGN_V_CENTER | ALIGN_V_BOTTOM) )
	{
		yp = ye - ys + 1;
		yp -= char_height;
		if ( yp < 0 ) return;
	}

	if ( align & ALIGN_V_CENTER ) yp >>= 1;
	yp += ys;

	sl = GUI_UTF8_to_Unicode(txt->str, strlen(txt->str), &unicode);
	if ( sl <= 0 ) return;

	txt_str = unicode;

	xp = xe - xs + 1;
	xp -= char_width*sl;
	xp -= char_h_space*(sl-1);

	if ( xp < 0 ) goto exit;

	if ( align & ALIGN_H_LEFT ) xp = 0;
	else if ( align & ALIGN_H_CENTER ) xp >>= 1;
	xp += xs;

	while( (*txt_str != 0) )
	{
		/*----------------------------------*/
		/* Draw one char                    */
		/*----------------------------------*/
		bt = *txt_str;
		UG_GetCharBitmap(UG_GetFontContext(), bt ,&charBitmap);
		yo = yp;
		bitsOffs = 0;
		CAL_BitStream(charBitmap.bitsData);
#if debug
		printk("bt 0x%x : char_height %d char_width %d bitsPerPixel %d \n",bt,char_height,char_width,charBitmap.bitsPerPixel);
#endif
		for(j = 0; j < char_height; j++ )
		{
			xo = xp;
			CAL_SeekBits(charBitmap.bitsData, bitsOffs);
			for( i= 0 ;i < charBitmap.charWidth ;i++ )
			{

				CAL_NeedBits(charBitmap.bitsPerPixel);

				bitsVal = CAL_ReadBits(charBitmap.bitsPerPixel);
						  CAL_DropBits(charBitmap.bitsPerPixel);

				color = UG_BlendPixel_RGB_565(txt->bc,txt->fc, 0xFF * bitsVal / (charBitmap.grayLevel  - 1));

				gui->pset(xo,yo,color);
				xo++;
			}
			yo++;
			bitsOffs += charBitmap.frameRect.w * charBitmap.bitsPerPixel;
		}
		/*----------------------------------*/
		xp += charBitmap.charWidth + char_h_space;
		txt_str++;
	}

exit:
	if ( unicode ) {
		mem_free(unicode);
	}
}

void UG_DrawBMP_Ext( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, void *bmp, UG_S16 stride)
{
	UG_GUI* gui = UG_GetGui();
	if( ((UG_RESULT(*)(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR *c, UG_S16 stride))
		gui->driver[DRIVER_DRAW_FRAME].driver)(x1, y1, x2, y2, bmp, stride) == UG_RESULT_OK ) return;

}

void UG_FillFrame_Ext(u16_t x1, u16_t y1, u16_t x2, u16_t y2, u32_t color)
{
	UG_GUI* gui = UG_GetGui();
	if( ((UG_RESULT(*)(u16_t x1, u16_t y1, u16_t x2, u16_t y2, u32_t color))
		gui->driver[DRIVER_FILL_FRAME].driver)(x1, y1, x2, y2,color) == UG_RESULT_OK ) return;

}

UG_RESULT UG_GetImg_Info(const char* img_id, UG_BMP* info)
{
	void *img_ptr = NULL;

	if(sd_fmap(img_id,(void**)&img_ptr, NULL)) {
		printk("boot logo not find \n");
		return UG_RESULT_FAIL;
	}
	info->p = (void *)(img_ptr + 32);
	info->width = *(u32_t *)(img_ptr + 4);
	info->height = *(u32_t *)(img_ptr + 8);
	info->bpp = BMP_BPP_16;
	info->colors = BMP_RGB565;
	//printk("img.width %d img.height %d img.bpp %d img.colors %d \n",info->width,info->height,info->bpp,info->colors);
	return UG_RESULT_OK;
}
