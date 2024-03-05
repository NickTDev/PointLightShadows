//Author: Eric Winebrenner

#include "Mesh.h"
namespace ew {
	void Mesh::Load(MeshData* meshData) {

		glGenVertexArrays(1, &mVAO);
		glBindVertexArray(mVAO);

		glGenBuffers(1, &mVBO);
		glBindBuffer(GL_ARRAY_BUFFER, mVBO);
		glBufferData(GL_ARRAY_BUFFER, meshData->vertices.size() * sizeof(Vertex), &meshData->vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &mEBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, meshData->indices.size() * sizeof(unsigned int), &meshData->indices[0], GL_STATIC_DRAW);

		//Position
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)(offsetof(Vertex, position)));
		glEnableVertexAttribArray(0);

		//Normal
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)(offsetof(Vertex, normal)));
		glEnableVertexAttribArray(1);

		//Texture Coordinate (UV)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)(offsetof(Vertex, uv)));
		glEnableVertexAttribArray(2);

		//Tangeant
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (const void*)(offsetof(Vertex, tangent)));
		glEnableVertexAttribArray(3);

		mNumIndices = (GLsizei)meshData->indices.size();
		mNumVertices = (GLsizei)meshData->vertices.size();
	}

	Mesh::~Mesh()
	{
		glDeleteVertexArrays(1, &mVAO);
		glDeleteBuffers(1, &mVBO);
		glDeleteBuffers(1, &mEBO);
	}

	void Mesh::draw()
	{
		glBindVertexArray(mVAO);
		glDrawElements(GL_TRIANGLES, mNumIndices, GL_UNSIGNED_INT, 0);
	}

}
