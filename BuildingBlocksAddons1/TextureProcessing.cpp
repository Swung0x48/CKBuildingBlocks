/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
//
//              TextureProcessing
//
/////////////////////////////////////////////////////
/////////////////////////////////////////////////////
#include "CKAll.h"
#include "TextureProcessing.h"
#include "VxSIMD.h"

#include <string.h>

// Convolutions Matrix
// Gaussian Blur Matrix
int CMAntialias[3][3] = {1, 2, 1, 2, 8, 2, 1, 2, 1};
// Gaussian Blur Matrix
int CMGaussianBlur[3][3] = {2, 4, 2, 4, 0, 4, 2, 4, 2};
// Gaussian Blur Matrix
int CMUniformBlur[3][3] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
// Gaussian Blur Matrix
int CMDirectionnal[3][3] = {4, 3, 2, 3, 2, 2, 2, 1, 0};
// Emboss Matrix
int CMEmboss[3][3] = {-1, -1, -1, -1, 8, -1, -1, -1, -1};
// Passe haut Matrix
int CMHighPass[3][3] = {0, -1, 0, -1, 5, -1, 0, -1, 0};
// Passe bas Matrix
int CMLowPass[3][3] = {0, 1, 0, 1, 3, 1, 0, 1, 0};

// Decal Matrices
int CMDecalLeft[3][3] = {0, 0, 0, 1, 0, 0, 0, 0, 0};
int CMDecalRight[3][3] = {0, 0, 0, 0, 0, 1, 0, 0, 0};
int CMDecalTop[3][3] = {0, 0, 0, 0, 0, 0, 0, 1, 0};
int CMDecalBottom[3][3] = {0, 1, 0, 0, 0, 0, 0, 0, 0};
int CMDecalNone[3][3] = {0, 0, 0, 0, 1, 0, 0, 0, 0};
int CMDecalBR[3][3] = {1, 0, 0, 0, 0, 0, 0, 0, 0};
int CMDecalBL[3][3] = {0, 0, 1, 0, 0, 0, 0, 0, 0};
int CMDecalTL[3][3] = {0, 0, 0, 0, 0, 0, 0, 0, 1};
int CMDecalTR[3][3] = {0, 0, 0, 0, 0, 0, 1, 0, 0};

CKDWORD *g_Buffer = NULL;
int g_Size = 0;

namespace
{
inline int ClampToByte(int value)
{
    if (value < 0)
        return 0;
    if (value > 255)
        return 255;
    return value;
}

inline int ShiftRightSigned8(int value)
{
    if (value >= 0)
        return value >> 8;
    return -(((-value) + 255) >> 8);
}

inline void DecodeColor(CKDWORD color, int &r, int &g, int &b, int &a)
{
    r = ColorGetRed(color);
    g = ColorGetGreen(color);
    b = ColorGetBlue(color);
    a = ColorGetAlpha(color);
}

inline CKDWORD EncodeColor(int r, int g, int b, int a)
{
    return RGBAITOCOLOR(ClampToByte(r), ClampToByte(g), ClampToByte(b), ClampToByte(a));
}

inline CKDWORD SampleWithBorder(const CKDWORD *data, int width, int height, int x, int y, CKDWORD borderColor)
{
    if (x < 0 || y < 0 || x >= width || y >= height)
        return borderColor;
    return data[y * width + x];
}

inline void DecodeKernel(const short *matrixData, int kernel[3][3])
{
    if (!matrixData)
    {
        kernel[0][0] = 0;
        kernel[0][1] = 0;
        kernel[0][2] = 0;
        kernel[1][0] = 0;
        kernel[1][1] = 256;
        kernel[1][2] = 0;
        kernel[2][0] = 0;
        kernel[2][1] = 0;
        kernel[2][2] = 0;
        return;
    }

    // Matrix layout matches the table created in ConvolveTexture3x3().
    kernel[0][0] = matrixData[1];
    kernel[0][1] = matrixData[0];
    kernel[0][2] = matrixData[4];
    kernel[1][0] = matrixData[9];
    kernel[1][1] = matrixData[8];
    kernel[1][2] = matrixData[12];
    kernel[2][0] = matrixData[17];
    kernel[2][1] = matrixData[16];
    kernel[2][2] = matrixData[20];
}
} // namespace

void ConvolveTexture3x3(CKTexture *texture, int kernel[3][3], int scale)
{
    if (!texture)
        return;

    if (!scale)
    {
        for (int a = 0; a < 3; ++a)
            for (int b = 0; b < 3; ++b)
                scale += kernel[a][b];
    }
    if (!scale)
        scale = 1;

    const int width = texture->GetWidth();
    const int height = texture->GetHeight();
    if (width <= 0 || height <= 0)
        return;

    CKDWORD *image = reinterpret_cast<CKDWORD *>(texture->LockSurfacePtr());
    if (!image)
        return;

    short normalized[3][3];
    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            normalized[a][b] = static_cast<short>((kernel[a][b] * 256) / scale);

    short kernelData[24] = {
        normalized[0][1], normalized[0][0], normalized[0][1], normalized[0][0], normalized[0][2], normalized[0][2], normalized[0][2], normalized[0][2],
        normalized[1][1], normalized[1][0], normalized[1][1], normalized[1][0], normalized[1][2], normalized[1][2], normalized[1][2], normalized[1][2],
        normalized[2][1], normalized[2][0], normalized[2][1], normalized[2][0], normalized[2][2], normalized[2][2], normalized[2][2], normalized[2][2]};

    const int paddedSize = (width + 2) * (height + 2);
    const int dstSize = width * height;

    if (!g_Buffer || paddedSize > g_Size)
    {
        if (g_Buffer)
            delete[] g_Buffer;
        g_Buffer = new CKDWORD[paddedSize];
        g_Size = paddedSize;
    }

    CKDWORD *padded = g_Buffer;
    const int paddedStride = width + 2;
    const int rowBytes = width * static_cast<int>(sizeof(CKDWORD));

    for (int y = 0; y < height; ++y)
    {
        const CKDWORD *srcRow = image + y * width;
        CKDWORD *dstRow = padded + (y + 1) * paddedStride + 1;
        dstRow[-1] = srcRow[0];
        memcpy(dstRow, srcRow, rowBytes);
        dstRow[width] = srcRow[width - 1];
    }

    memcpy(padded + 1, image, rowBytes);
    memcpy(padded + (height + 1) * paddedStride + 1, image + (height - 1) * width, rowBytes);

    padded[0] = image[0];
    padded[width + 1] = image[width - 1];
    padded[paddedSize - 1] = image[dstSize - 1];
    padded[paddedSize - paddedStride] = image[dstSize - width];

    ConvolvePaddedPixels3x3(padded, image, width, height, kernelData);
    texture->ReleaseSurfacePtr();
}

void BlendPixelBuffersScalar(CKDWORD *output, const CKDWORD *first, const CKDWORD *second, int pixelCount, float blendFactor)
{
    CKDWORD *out = output;
    const CKDWORD *lhs = first;
    const CKDWORD *rhs = second;
    const float rhsFactor = blendFactor;
    const float lhsFactor = (1.0f - blendFactor);

    while (pixelCount-- > 0)
    {
        const CKDWORD c0 = *lhs++;
        const CKDWORD c1 = *rhs++;
        CKDWORD ag = static_cast<CKDWORD>(((c0 & 0xFF00FF00) >> 2) * lhsFactor + ((c1 & 0xFF00FF00) >> 2) * rhsFactor);
        CKDWORD rb = static_cast<CKDWORD>(((c0 & 0x00FF00FF)) * lhsFactor + ((c1 & 0x00FF00FF)) * rhsFactor);
        ag = (ag << 2) & 0xFF00FF00;
        rb &= 0x00FF00FF;
        *out++ = ag | rb;
    }
}

void BlendPixelBuffers(CKDWORD *output, const CKDWORD *first, const CKDWORD *second, int pixelCount, float blendFactor)
{
    BlendPixelBuffersScalar(output, first, second, pixelCount, blendFactor);
}

void ConvolvePaddedPixels3x3(const CKDWORD *paddedSource, CKDWORD *destination, int width, int height, const short *kernelData)
{
    if (!paddedSource || !destination || width <= 0 || height <= 0)
        return;

    int kernel[3][3];
    DecodeKernel(kernelData, kernel);

    const int paddedStride = width + 2;
    for (int y = 0; y < height; ++y)
    {
        const CKDWORD *row0 = paddedSource + y * paddedStride;
        const CKDWORD *row1 = row0 + paddedStride;
        const CKDWORD *row2 = row1 + paddedStride;

        for (int x = 0; x < width; ++x)
        {
            int sumR = 0;
            int sumG = 0;
            int sumB = 0;
            int sumA = 0;

            int r, g, b, a;
            DecodeColor(row0[x], r, g, b, a);
            sumR += r * kernel[0][0];
            sumG += g * kernel[0][0];
            sumB += b * kernel[0][0];
            sumA += a * kernel[0][0];

            DecodeColor(row0[x + 1], r, g, b, a);
            sumR += r * kernel[0][1];
            sumG += g * kernel[0][1];
            sumB += b * kernel[0][1];
            sumA += a * kernel[0][1];

            DecodeColor(row0[x + 2], r, g, b, a);
            sumR += r * kernel[0][2];
            sumG += g * kernel[0][2];
            sumB += b * kernel[0][2];
            sumA += a * kernel[0][2];

            DecodeColor(row1[x], r, g, b, a);
            sumR += r * kernel[1][0];
            sumG += g * kernel[1][0];
            sumB += b * kernel[1][0];
            sumA += a * kernel[1][0];

            DecodeColor(row1[x + 1], r, g, b, a);
            sumR += r * kernel[1][1];
            sumG += g * kernel[1][1];
            sumB += b * kernel[1][1];
            sumA += a * kernel[1][1];

            DecodeColor(row1[x + 2], r, g, b, a);
            sumR += r * kernel[1][2];
            sumG += g * kernel[1][2];
            sumB += b * kernel[1][2];
            sumA += a * kernel[1][2];

            DecodeColor(row2[x], r, g, b, a);
            sumR += r * kernel[2][0];
            sumG += g * kernel[2][0];
            sumB += b * kernel[2][0];
            sumA += a * kernel[2][0];

            DecodeColor(row2[x + 1], r, g, b, a);
            sumR += r * kernel[2][1];
            sumG += g * kernel[2][1];
            sumB += b * kernel[2][1];
            sumA += a * kernel[2][1];

            DecodeColor(row2[x + 2], r, g, b, a);
            sumR += r * kernel[2][2];
            sumG += g * kernel[2][2];
            sumB += b * kernel[2][2];
            sumA += a * kernel[2][2];

            destination[y * width + x] = EncodeColor(ShiftRightSigned8(sumR),
                                                     ShiftRightSigned8(sumG),
                                                     ShiftRightSigned8(sumB),
                                                     ShiftRightSigned8(sumA));
        }
    }
}

void SwapPixelBuffers(CKDWORD *first, CKDWORD *second, CKDWORD pixelCount)
{
    if (!first || !second || pixelCount == 0)
        return;

#if defined(VX_SIMD_SSE2)
    CKDWORD i = 0;
    for (; i + 3 < pixelCount; i += 4)
    {
        __m128i a = _mm_loadu_si128(reinterpret_cast<const __m128i *>(first + i));
        __m128i b = _mm_loadu_si128(reinterpret_cast<const __m128i *>(second + i));
        _mm_storeu_si128(reinterpret_cast<__m128i *>(first + i), b);
        _mm_storeu_si128(reinterpret_cast<__m128i *>(second + i), a);
    }
    for (; i < pixelCount; ++i)
    {
        CKDWORD tmp = first[i];
        first[i] = second[i];
        second[i] = tmp;
    }
#else
    for (CKDWORD i = 0; i < pixelCount; ++i)
    {
        CKDWORD tmp = first[i];
        first[i] = second[i];
        second[i] = tmp;
    }
#endif
}

void ConvolveTemporalPixels3x3(const CKDWORD *currentFrame, CKDWORD *previousFrame, int width, int height, const short *kernelData, int damping)
{
    if (!currentFrame || !previousFrame || width <= 0 || height <= 0)
        return;

    int kernel[3][3];
    DecodeKernel(kernelData, kernel);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int convR = 0;
            int convG = 0;
            int convB = 0;
            int convA = 0;

            for (int ky = -1; ky <= 1; ++ky)
            {
                const int sy = (y + ky < 0) ? 0 : ((y + ky >= height) ? (height - 1) : (y + ky));
                for (int kx = -1; kx <= 1; ++kx)
                {
                    const int sx = (x + kx < 0) ? 0 : ((x + kx >= width) ? (width - 1) : (x + kx));
                    int r, g, b, a;
                    DecodeColor(currentFrame[sy * width + sx], r, g, b, a);
                    const int w = kernel[ky + 1][kx + 1];
                    convR += r * w;
                    convG += g * w;
                    convB += b * w;
                    convA += a * w;
                }
            }

            int prevR, prevG, prevB, prevA;
            DecodeColor(previousFrame[y * width + x], prevR, prevG, prevB, prevA);

            int outR = ClampToByte(ShiftRightSigned8(convR) - prevR);
            int outG = ClampToByte(ShiftRightSigned8(convG) - prevG);
            int outB = ClampToByte(ShiftRightSigned8(convB) - prevB);
            int outA = ClampToByte(ShiftRightSigned8(convA) - prevA);

            if (damping > 0 && damping < 31)
            {
                outR -= (outR >> damping);
                outG -= (outG >> damping);
                outB -= (outB >> damping);
                outA -= (outA >> damping);
            }

            previousFrame[y * width + x] = EncodeColor(outR, outG, outB, outA);
        }
    }
}

void ApplyWaterRipple(CKDWORD *currentFrame, CKDWORD *previousFrame, int width, int height, CKDWORD borderColor, int damping)
{
    if (!currentFrame || !previousFrame || width <= 0 || height <= 0)
        return;

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int lR, lG, lB, lA;
            int rR, rG, rB, rA;
            int uR, uG, uB, uA;
            int dR, dG, dB, dA;
            int pR, pG, pB, pA;

            DecodeColor(SampleWithBorder(currentFrame, width, height, x - 1, y, borderColor), lR, lG, lB, lA);
            DecodeColor(SampleWithBorder(currentFrame, width, height, x + 1, y, borderColor), rR, rG, rB, rA);
            DecodeColor(SampleWithBorder(currentFrame, width, height, x, y - 1, borderColor), uR, uG, uB, uA);
            DecodeColor(SampleWithBorder(currentFrame, width, height, x, y + 1, borderColor), dR, dG, dB, dA);
            DecodeColor(previousFrame[y * width + x], pR, pG, pB, pA);

            int outR = ClampToByte((((lR + rR + uR + dR) >> 2) << 1) - pR);
            int outG = ClampToByte((((lG + rG + uG + dG) >> 2) << 1) - pG);
            int outB = ClampToByte((((lB + rB + uB + dB) >> 2) << 1) - pB);
            int outA = ClampToByte((((lA + rA + uA + dA) >> 2) << 1) - pA);

            if (damping > 0 && damping < 31)
            {
                outR -= (outR >> damping);
                outG -= (outG >> damping);
                outB -= (outB >> damping);
                outA -= (outA >> damping);
            }

            previousFrame[y * width + x] = EncodeColor(outR, outG, outB, outA);
        }
    }
}
