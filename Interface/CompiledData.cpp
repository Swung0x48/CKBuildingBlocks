#include "CompiledData.h"

CompiledTextData::CompiledTextData()
{
	m_VertexCapacity = 0;
	PrepareStructure();
}

CompiledTextData::~CompiledTextData()
{
	ClearStructure();
}

void CompiledTextData::PrepareStructure()
{
	memset(&m_DrawPrimData, 0, sizeof(VxDrawPrimitiveData));
#if CKVERSION == 0x13022002 || CKVERSION == 0x05082002
	m_DrawPrimData.PositionStride = sizeof(VxVector4);
	m_DrawPrimData.NormalStride = sizeof(VxVector);
	m_DrawPrimData.SpecularColorStride = sizeof(CKDWORD);
	m_DrawPrimData.ColorStride = sizeof(CKDWORD);
	m_DrawPrimData.TexCoordStride = sizeof(VxUV);
	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		m_DrawPrimData.TexCoordStrides[i] = sizeof(VxUV);
	}
#else
	m_DrawPrimData.Positions.Stride = sizeof(VxVector4);
	m_DrawPrimData.Normals.Stride = sizeof(VxVector);
	m_DrawPrimData.SpecularColors.Stride = sizeof(CKDWORD);
	m_DrawPrimData.Colors.Stride = sizeof(CKDWORD);
	m_DrawPrimData.TexCoord.Stride = sizeof(VxUV);
	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		m_DrawPrimData.TexCoords[i].Stride = sizeof(VxUV);
	}
#endif

	memcpy(&m_DrawPrimDataUser, &m_DrawPrimData, sizeof(VxDrawPrimitiveData));
}

void CompiledTextData::ClearStructure()
{
#if CKVERSION == 0x13022002 || CKVERSION == 0x05082002
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.PositionPtr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.NormalPtr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.ColorPtr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.SpecularColorPtr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoordPtr);
	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoordPtrs[i]);
	}
#else
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.Positions.Ptr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.Normals.Ptr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.Colors.Ptr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.SpecularColors.Ptr);
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoord.Ptr);
	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoords[i].Ptr);
	}
#endif

	m_VertexCapacity = 0;
}

void CompiledTextData::ExtendStructure(int nc)
{
	void *temp;
	const int oldcount = m_DrawPrimData.VertexCount;

	if (nc <= m_VertexCapacity)
	{
		m_DrawPrimData.VertexCount = nc;
		return;
	}

	int newCapacity = m_VertexCapacity ? (m_VertexCapacity * 2) : nc;
	if (newCapacity < nc)
		newCapacity = nc;

#if CKVERSION == 0x13022002 || CKVERSION == 0x05082002
	temp = new CKBYTE[newCapacity * sizeof(CKDWORD)];
	memcpy(temp, m_DrawPrimData.ColorPtr, oldcount * sizeof(CKDWORD));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.ColorPtr);
	m_DrawPrimData.ColorPtr = temp;

	temp = new CKBYTE[newCapacity * sizeof(CKDWORD)];
	memcpy(temp, m_DrawPrimData.SpecularColorPtr, oldcount * sizeof(CKDWORD));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.SpecularColorPtr);
	m_DrawPrimData.SpecularColorPtr = temp;

	temp = new CKBYTE[newCapacity * sizeof(VxVector)];
	memcpy(temp, m_DrawPrimData.NormalPtr, oldcount * sizeof(VxVector));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.NormalPtr);
	m_DrawPrimData.NormalPtr = temp;

	temp = new CKBYTE[newCapacity * sizeof(VxVector4)];
	memcpy(temp, m_DrawPrimData.PositionPtr, oldcount * sizeof(VxVector4));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.PositionPtr);
	m_DrawPrimData.PositionPtr = temp;

	temp = new CKBYTE[newCapacity * sizeof(VxUV)];
	memcpy(temp, m_DrawPrimData.TexCoordPtr, oldcount * sizeof(VxUV));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoordPtr);
	m_DrawPrimData.TexCoordPtr = temp;

	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		temp = new CKBYTE[newCapacity * sizeof(VxUV)];
		memcpy(temp, m_DrawPrimData.TexCoordPtrs[i], oldcount * sizeof(VxUV));
		delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoordPtrs[i]);
		m_DrawPrimData.TexCoordPtrs[i] = temp;
	}
#else
	temp = new CKBYTE[newCapacity * sizeof(CKDWORD)];
	memcpy(temp, m_DrawPrimData.Colors.Ptr, oldcount * sizeof(CKDWORD));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.Colors.Ptr);
	m_DrawPrimData.Colors.Ptr = temp;

	temp = new CKBYTE[newCapacity * sizeof(CKDWORD)];
	memcpy(temp, m_DrawPrimData.SpecularColors.Ptr, oldcount * sizeof(CKDWORD));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.SpecularColors.Ptr);
	m_DrawPrimData.SpecularColors.Ptr = temp;

	temp = new CKBYTE[newCapacity * sizeof(VxVector)];
	memcpy(temp, m_DrawPrimData.Normals.Ptr, oldcount * sizeof(VxVector));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.Normals.Ptr);
	m_DrawPrimData.Normals.Ptr = temp;

	temp = new CKBYTE[newCapacity * sizeof(VxVector4)];
	memcpy(temp, m_DrawPrimData.Positions.Ptr, oldcount * sizeof(VxVector4));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.Positions.Ptr);
	m_DrawPrimData.Positions.Ptr = temp;

	temp = new CKBYTE[newCapacity * sizeof(VxUV)];
	memcpy(temp, m_DrawPrimData.TexCoord.Ptr, oldcount * sizeof(VxUV));
	delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoord.Ptr);
	m_DrawPrimData.TexCoord.Ptr = temp;

	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		temp = new CKBYTE[newCapacity * sizeof(VxUV)];
		memcpy(temp, m_DrawPrimData.TexCoords[i].Ptr, oldcount * sizeof(VxUV));
		delete[] static_cast<CKBYTE *>(m_DrawPrimData.TexCoords[i].Ptr);
		m_DrawPrimData.TexCoords[i].Ptr = temp;
	}
#endif

	m_VertexCapacity = newCapacity;
	m_DrawPrimData.VertexCount = nc;
}

void CompiledTextData::Reset()
{
	m_DrawPrimData.VertexCount = 0;
	m_DrawPrimDataUser.VertexCount = 0;
	m_Indices.Resize(0);
	m_DrawZone = VxRect(0.0f, 0.0f, 0.0f, 0.0f);
}

int CompiledTextData::GetIndexCount() const
{
	return m_Indices.Size();
}

const CKWORD *CompiledTextData::GetIndexPtr() const
{
	return m_Indices.Begin();
}

VxDrawPrimitiveData *CompiledTextData::GetDrawPrimitiveData()
{
	return &m_DrawPrimData;
}

VxDrawPrimitiveData *CompiledTextData::GetStructure(CKRST_DPFLAGS Flags, int VertexCount)
{
	int oldsize = m_DrawPrimData.VertexCount;
	ExtendStructure(oldsize + VertexCount);

	// vertices count
	m_DrawPrimDataUser.VertexCount = VertexCount;

#if CKVERSION == 0x13022002 || CKVERSION == 0x05082002
	// Ptrs
	m_DrawPrimDataUser.ColorPtr = (CKBYTE *)m_DrawPrimData.ColorPtr + oldsize * m_DrawPrimData.ColorStride;
	m_DrawPrimDataUser.SpecularColorPtr = (CKBYTE *)m_DrawPrimData.SpecularColorPtr + oldsize * m_DrawPrimData.SpecularColorStride;
	m_DrawPrimDataUser.NormalPtr = (CKBYTE *)m_DrawPrimData.NormalPtr + oldsize * m_DrawPrimData.NormalStride;
	m_DrawPrimDataUser.PositionPtr = (CKBYTE *)m_DrawPrimData.PositionPtr + oldsize * m_DrawPrimData.PositionStride;
	m_DrawPrimDataUser.TexCoordPtr = (CKBYTE *)m_DrawPrimData.TexCoordPtr + oldsize * m_DrawPrimData.TexCoordStride;
	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		m_DrawPrimDataUser.TexCoordPtrs[i] = (CKBYTE *)m_DrawPrimData.TexCoordPtrs[i] + oldsize * m_DrawPrimData.TexCoordStrides[i];
	}

	// Flags
	// Warning : this means the render is done with options of the last call to get structure
	m_DrawPrimData.Flags = Flags;
	m_DrawPrimDataUser.Flags = Flags;
	if (!(Flags & CKRST_DP_SPECULAR))
		m_DrawPrimDataUser.SpecularColorPtr = NULL;
#else
	// Ptrs
	m_DrawPrimDataUser.Colors.Ptr = (CKBYTE *)m_DrawPrimData.Colors.Ptr + oldsize * m_DrawPrimData.Colors.Stride;
	m_DrawPrimDataUser.SpecularColors.Ptr = (CKBYTE *)m_DrawPrimData.SpecularColors.Ptr + oldsize * m_DrawPrimData.SpecularColors.Stride;
	m_DrawPrimDataUser.Normals.Ptr = (CKBYTE *)m_DrawPrimData.Normals.Ptr + oldsize * m_DrawPrimData.Normals.Stride;
	m_DrawPrimDataUser.Positions.Ptr = (CKBYTE *)m_DrawPrimData.Positions.Ptr + oldsize * m_DrawPrimData.Positions.Stride;
	m_DrawPrimDataUser.TexCoord.Ptr = (CKBYTE *)m_DrawPrimData.TexCoord.Ptr + oldsize * m_DrawPrimData.TexCoord.Stride;
	for (int i = 0; i < CKRST_MAX_STAGES - 1; ++i)
	{
		m_DrawPrimDataUser.TexCoords[i].Ptr = (CKBYTE *)m_DrawPrimData.TexCoords[i].Ptr + oldsize * m_DrawPrimData.TexCoords[i].Stride;
	}

	// Flags
	// Warning : this means the render is done with options of the last call to get structure
	m_DrawPrimData.Flags = Flags;
	m_DrawPrimDataUser.Flags = Flags;
	if (!(Flags & CKRST_DP_SPECULAR))
		m_DrawPrimDataUser.SpecularColors.Ptr = NULL;
#endif

	return &m_DrawPrimDataUser;
}

CKWORD *CompiledTextData::GetIndices(int IndicesCount)
{
	int oldsize = m_Indices.Size();
	m_Indices.Resize(oldsize + IndicesCount);
	return m_Indices.Begin() + oldsize;
}

void CompiledTextData::PatchIndices(int initialsize, int newsize)
{
	int vertoffset = m_DrawPrimData.VertexCount - m_DrawPrimDataUser.VertexCount;
	int start = m_Indices.Size() - initialsize;
	int offset = initialsize - newsize;
	for (CKWORD *it = m_Indices.Begin() + start; it != m_Indices.End() - offset; ++it)
	{
		*it = *it + vertoffset;
	}
	m_Indices.Resize(start + newsize);
}

void CompiledTextData::Render(CKRenderContext *dev)
{
	dev->DrawPrimitive(VX_TRIANGLELIST, m_Indices.Begin(), m_Indices.Size(), &m_DrawPrimData);
}
