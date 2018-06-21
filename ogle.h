#ifndef __OGLE_H_
#define __OGLE_H_

#include <gl/gl.h>

#include "../../MainLib/InterceptPluginInterface.h"

#undef min
#undef max

#include "mtl/mtl.h"
#include "mtl/matrix.h"

#include <vector>
#include <deque>
#include <map>
#include <string>
#include <array>


#define OGLE_BIND_BUFFERS_ALL_FRAMES 1
#define OGLE_CAPTURE_BUFFERS_ALL_FRAMES 0

#define OGLE_N_BUFFERS (4096*2)
#define OGLE_N_VAOS (4096)

#include "Ptr/Interface.h"
#include "Ptr/Ptr.h"

class ObjFile;

class OGLE : public Interface {

public:

	// Classes for 4x1 Vectors,
	// the typical mathematical components of OpenGL calculations

    typedef mtl::dense1D < float > Vector;

	//////////////////////////////////////////////////////////////////////
	// OGLE::Vertex -- Class for OpenGL Vertexinate
	//////////////////////////////////////////////////////////////////////

	class Vertex : public Interface {
		public:
			GLfloat x,y,z,w;
			GLbyte size;

			inline Vertex();
			inline Vertex(Vector v);
			inline Vertex(const GLfloat *v, GLint n);
			inline Vertex(GLfloat _x, GLfloat _y, GLfloat _z = 0, GLfloat _w = 1);

			inline void init(GLfloat _x = 0, GLfloat _y = 0, GLfloat _z = 0, GLfloat _w = 1);
			inline void fromVector(Vector v);

			inline Vector toVector();
	};
	typedef Ptr<Vertex> VertexPtr;

	//////////////////////////////////////////////////////////////////////
	// OGLE::Element -- Class for an OpenGL 'Element' which can have a
	// Vertex (location), as well as a Normal and a Texture coordinate
	//////////////////////////////////////////////////////////////////////

	class Element : public Interface {
		public:
			Element() { v = 0; t = 0; n = 0; }

			Element(VertexPtr _v, VertexPtr _t = 0, VertexPtr _n = 0) {
				v = _v; t = _t; n = _n;
			}

			VertexPtr v,t,n;
	};
	typedef Ptr<Element> ElementPtr;

	//////////////////////////////////////////////////////////////////////
	// OGLE::ElementSet -- Class for a set of OpenGL vertices and indices
	//////////////////////////////////////////////////////////////////////

	class ElementSet : public Interface {
		public:
			ElementSet(GLenum _mode);

  			// add a Element with just a location Vertex
			void addElement(const GLfloat V[], GLsizei dim);
			void addElement(VertexPtr V);
			void addElement(ElementPtr E);

			std::vector<ElementPtr> elements;

			GLenum mode;
			std::vector<GLuint> indices;
	};
	typedef Ptr<ElementSet> ElementSetPtr;


	//////////////////////////////////////////////////////////////////////
	// OGLE::Blob -- Class for polymorphic storage of small primitive types
	//////////////////////////////////////////////////////////////////////

	class Blob : public Interface {
		private:
			char d[16];

		public:
			Blob(const GLint v) { *((GLint *)d) = v; }
			const GLint toInt() { return *((GLint *)d); }
			const GLuint toUInt() { return *((GLuint *)d); }
			const GLsizei toSizeI() { return *((GLsizei *)d); }

			Blob(const GLboolean v) { *((GLboolean *)d) = v; }
			const GLboolean toBool() { return *((GLboolean *)d); }

			Blob(const GLfloat v) { *((GLfloat *)d) = v; }
			const GLfloat toFloat() { return *((GLfloat *)d); }

			Blob(const GLenum v) { *((GLenum *)d) = v; }
			const GLenum toEnum() { return *((GLenum *)d); }

			Blob(const GLvoid *v) {*((const GLvoid **)d) = v;}
			const GLvoid * toVoidP() { return *((const GLvoid **)d);}
	};

	typedef Ptr<Blob> BlobPtr;

	//////////////////////////////////////////////////////////////////////
	// OGLE::Buffer -- Class for a buffer
	//////////////////////////////////////////////////////////////////////

	class Buffer : public Interface {
		public:
			GLvoid *ptr;
			GLsizeiptr size;

			GLvoid *map;
			GLenum mapAccess;

			inline Buffer(const GLvoid *_ptr, GLsizeiptr _size);
			inline ~Buffer();
	};

	typedef Ptr<Buffer> BufferPtr;

	//////////////////////////////////////////////////////////////////////
	// OGLE::AttribDesc -- Struct for an attribute description
	//////////////////////////////////////////////////////////////////////

	struct AttribDesc {
		GLboolean enabled;
		GLuint buffer_index;
		GLint size;
		GLenum type;
		GLboolean normalized;
		GLsizei stride;
		const GLvoid *pointer;

		inline AttribDesc();
	};

	//////////////////////////////////////////////////////////////////////
	// OGLE::VAO -- Class for a vertex array object
	//////////////////////////////////////////////////////////////////////

	class VAO : public Interface {
		public:
			std::array<AttribDesc, 16> attrib_descs;
			GLuint index_buffer_index;

			inline VAO();
	};

	typedef Ptr<VAO> VAOPtr;

	struct ltstr
	{
	  bool operator()(const char* s1, const char* s2) const
	  { return strcmp(s1, s2) < 0; }
	};


	//////////////////////////////////////////////////////////////////////
	// OGLE::Config -- Class for all OGLE config options
	//////////////////////////////////////////////////////////////////////

	struct Config {
		public:
			float scale;
			bool logFunctions;
			bool captureNormals;
			bool captureTexCoords;
			bool flipPolyStrips;
			map<const char*, bool, ltstr>polyTypesEnabled;

			static char *polyTypes[];
			static int nPolyTypes;

			inline Config();
	};


	////////////////////////////////////////////////////////////////////////
	// OGLE -- the class for actually doing the OpenGLExtraction
	////////////////////////////////////////////////////////////////////////


	OGLE(InterceptPluginCallbacks *_callBacks, const GLCoreDriver *_GLV);


	void startRecording(string _objFileName);
	void stopRecording();

	void addSet(ElementSetPtr set);
	void newSet(GLenum mode);

	void glBindVertexArray(GLuint array);
	void glEnableVertexAttribArray(GLuint index);
	void glDisableVertexAttribArray(GLuint index);
	void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);

	void glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
	void glDrawArrays (GLenum mode, GLint first, GLsizei count);
	void glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid *indices);
	void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex);

	void glLockArraysEXT(GLint first, GLsizei count);
	void glUnlockArraysEXT();
	void glBindBuffer(GLenum target, GLuint buffer);
	void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
	void glBufferStorage(GLenum target, GLsizeiptr size, const GLvoid *data, GLbitfield flags);
	void glBufferSubData(GLenum target, GLint offset, GLsizeiptr size, const GLvoid *data);
	void glMapBuffer(GLenum target, GLenum access);
	void glMapBufferPost(GLvoid *retValue);
	void glUnmapBuffer(GLenum target);

	void initFunctions();

	GLuint getBufferIndex(GLenum target);
	GLuint getVertexArrayIndex();
	VAOPtr getVertexArray();
	const GLbyte *OGLE::getBufferedArray(const GLbyte *array);
	const GLvoid *getBufferedIndices(const GLvoid *indices);
	void checkBuffers();

	bool isElementLocked(int index);



	InterceptPluginCallbacks *callBacks;
    const GLCoreDriver       *GLV;                  //The core OpenGL driver
	std::map<const char*, BlobPtr, ltstr> glState;

	std::vector<BufferPtr> buffers;
	std::vector<VAOPtr> vaos;

	bool extensionVBOSupported;
	void    (GLAPIENTRY *iglGetBufferSubData) (GLenum, GLint, GLsizeiptr, GLvoid *);

    Ptr<ElementSet> currSet;
	std::vector<ElementSetPtr> sets;


	string objFileName;
	int objIndex;
	Ptr<ObjFile> objFile;



	//////////////////////////////////////////////////////////////////////////////
	// Static methods and variables for the OGLE class

	static void init();

	static GLuint derefIndexArray(GLenum type, const GLvoid *indices, int i);

	static GLsizei glTypeSize(GLenum type);

	static FILE *LOG;
	static Config config;
};

typedef Ptr<OGLE> OGLEPtr;



/////////////////////////////////////////////////////////////////////////////////
//
// OGLE::Vertex Short/obvious Methods
//
/////////////////////////////////////////////////////////////////////////////////




OGLE::Vertex::Vertex() {
	init();
}

OGLE::Vertex::Vertex(GLfloat _x, GLfloat _y, GLfloat _z, GLfloat _w) {
	init(x,y,z,w);
}


void OGLE::Vertex::init(GLfloat _x, GLfloat _y, GLfloat _z, GLfloat _w) {
			x = _x; y = _y; z = _z; w = _w;
			size = 4;
}

OGLE::Vertex::Vertex(Vector v) {
	init();
	this->fromVector(v);
}

OGLE::Vertex::Vertex(const GLfloat *v, GLint n) {
	  init();
	  Vector V(n);
	  for(int i = 0; i < n; i++) {
		  V[i] = (float)v[i];
	  }
	  this->fromVector(V);
	  size = n;
}

void OGLE::Vertex::fromVector(Vector v) {
	switch (v.size()) {
	case 4: w = v[3];
	case 3: z = v[2];
	case 2: y = v[1];
	case 1: x = v[0];
	}
	size = (GLbyte)v.size();
}


OGLE::Vector OGLE::Vertex::toVector() {
	Vector v(4);
	v[0] = x; v[1] = y; v[2] = z; v[3] = w;
	return v;
}

OGLE::AttribDesc::AttribDesc() {
	enabled = false;
	buffer_index = 0;
	size = 4;
	type = GL_FLOAT;
	normalized = false;
	stride = 0;
	pointer = 0;
}

OGLE::VAO::VAO() {
	index_buffer_index = 0;
}

OGLE::Buffer::Buffer(const GLvoid *_ptr, GLsizeiptr _size) {
	size = _size;
	ptr = malloc(size);

	map = 0;
	mapAccess = 0;

	if(_ptr) {
		memcpy(ptr, _ptr, size);
	}
}

OGLE::Buffer::~Buffer() {
	if(ptr) free(ptr);
}



OGLE::Config::Config() : scale(1), logFunctions(0),
						 captureNormals(0), captureTexCoords(0),
						 flipPolyStrips(1) {
	for(int i = 0; i < OGLE::Config::nPolyTypes; i++) {
		const char *type = OGLE::Config::polyTypes[i];
		polyTypesEnabled[type] = 1;
	}
}


#endif // __OGLE_H_

