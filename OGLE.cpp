#include "stdafx.h"

#include "ogle.h"

#include "ObjFile.h"

#include "Ptr/Ptr.in"

#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 34962
#endif

#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER 34963
#endif

#ifndef GL_READ_ONLY
#define GL_READ_ONLY 35000
#endif

#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 35001
#endif

#ifndef GL_TEXTURE0
#define GL_TEXTURE0  33984
#endif


/////////////////////////////////////////////
// Static OGLE Variables
/////////////////////////////////////////////

char *OGLE::Config::polyTypes[] = {
	"TRIANGLES",
	"TRIANGLE_STRIP",
	"TRIANGLE_FAN",
	"QUADS",
	"QUAD_STRIP",
	"POLYGON"
};
int OGLE::Config::nPolyTypes = 6;

OGLE::Config OGLE::config;

FILE *OGLE::LOG = fopen("ogle.log", "w");


/////////////////////////////////////////////
// OGLE Initialization
/////////////////////////////////////////////

OGLE::OGLE(InterceptPluginCallbacks *_callBacks, const GLCoreDriver *_GLV) :
	currSet(0),
	callBacks(_callBacks),
	GLV(_GLV),
	buffers(OGLE_N_BUFFERS),
	vaos(OGLE_N_VAOS)
{
	currSet = 0;
}

void OGLE::startRecording(string _objFileName) {
	objFileName = _objFileName;
	objIndex = 0;
	objFile = new ObjFile(objFileName);
}

void OGLE::stopRecording() {
	objFile = 0;
	objFileName = "";
	objIndex = 0;
}



/////////////////////////////////////////////////////////////////////////////////
// OGLE's OpenGLFunctions
/////////////////////////////////////////////////////////////////////////////////

void OGLE::glBindVertexArray(GLuint array) {
	glState["GL_VERTEX_ARRAY_INDEX"] = new Blob(array);
}

void OGLE::glEnableVertexAttribArray(GLuint index) {
	VAOPtr vao = getVertexArray();
	if (vao)
		vao->attrib_descs[index].enabled = true;
}

void OGLE::glDisableVertexAttribArray(GLuint index) {
	VAOPtr vao = getVertexArray();
	if (vao)
		vao->attrib_descs[index].enabled = false;
}

void OGLE::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer) {
	VAOPtr vao = getVertexArray();
	if (vao) {
		AttribDesc &desc = vao->attrib_descs[index];
		desc.buffer_index = getBufferIndex(GL_ARRAY_BUFFER);
		desc.size = size;
		desc.type = type;
		desc.normalized = normalized;
		desc.stride = stride;
		desc.pointer = pointer;
	}
}

void OGLE::glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid *indices) {
	++objIndex;

    checkBuffers();

	indices = getBufferedIndices(indices);

	if (!indices) {
		fprintf(OGLE::LOG, "\tOGLE::glDrawElements: indices is null\n");
		return;
	}

	newSet(mode);

	if (!currSet)
		return;

	for(GLsizei i = 0; i < count; i++) {
		GLuint index = derefIndexArray(type, indices, i);
		currSet->indices.push_back(index);
	}

	addSet(currSet);
	currSet = 0;
}


void OGLE::glDrawArrays (GLenum mode, GLint first, GLsizei count) {
  ++objIndex;

  checkBuffers();

  newSet(mode);

  if (!currSet)
	  return;

  for(GLsizei i = 0; i < count; i++) {
	GLuint index = i + first;
	currSet->indices.push_back(index);
  }

  addSet(currSet);
  currSet = 0;
}

void OGLE::glDrawRangeElements(GLenum mode, GLuint start, GLuint end, GLsizei count,
							GLenum type, const GLvoid *indices) {
  ++objIndex;

  checkBuffers();

  indices = getBufferedIndices(indices);

  if (!indices) {
	  fprintf(OGLE::LOG, "\tOGLE::glDrawRangeElements: indices is null\n");
	  return;
  }

  newSet(mode);

  if (!currSet)
	  return;

  for(GLsizei i = 0; i < count; i++) {
	GLuint index = derefIndexArray(type, indices, i);
	if(index >= start && index <= end)
		currSet->indices.push_back(index);
  }

  addSet(currSet);
  currSet = 0;

}

void OGLE::glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices, GLint basevertex) {
	++objIndex;

    checkBuffers();

	indices = getBufferedIndices(indices);

	if (!indices) {
		fprintf(OGLE::LOG, "\tOGLE::glDrawElementsBaseVertex: indices is null\n");
		return;
	}

	newSet(mode);

	if (!currSet)
		return;

	for(GLsizei i = 0; i < count; i++) {
		GLuint index = derefIndexArray(type, indices, i);
		currSet->indices.push_back(index + basevertex);
	}

	addSet(currSet);
	currSet = 0;
}

void  OGLE::glLockArraysEXT(GLint first, GLsizei count) {
  glState["GL_LOCK_ARRAYS_FIRST"] = new OGLE::Blob(first);
  glState["GL_LOCK_ARRAYS_COUNT"] = new OGLE::Blob(count);

}

void  OGLE::glUnlockArraysEXT() {
  glState["GL_LOCK_ARRAYS_FIRST"] = 0;
  glState["GL_LOCK_ARRAYS_COUNT"] = 0;
}

void OGLE::glBindBuffer(GLenum target, GLuint buffer) {
	VAOPtr vao = 0;

	switch(target) {
		case GL_ARRAY_BUFFER:
			glState["GL_ARRAY_BUFFER_INDEX"] = new Blob(buffer); break;
		case GL_ELEMENT_ARRAY_BUFFER:
			glState["GL_ELEMENT_ARRAY_BUFFER_INDEX"] = new Blob(buffer);
			vao = getVertexArray();
			if (vao)
				vao->index_buffer_index = buffer;
			break;
		case GL_COPY_WRITE_BUFFER:
			glState["GL_COPY_WRITE_BUFFER_INDEX"] = new Blob(buffer); break;
	}
}

void OGLE::glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) {
	GLuint index = getBufferIndex(target);

	if(index) {
		BufferPtr buff = new Buffer(data, size);
		buffers[index] = buff;
	}
}

void OGLE::glBufferStorage(GLenum target, GLsizeiptr size, const GLvoid *data, GLbitfield flags) {
	GLuint index = getBufferIndex(target);

	if (index) {
		BufferPtr buff = new Buffer(data, size);
		buffers[index] = buff;
	}
}

void OGLE::glBufferSubData(GLenum target, GLint offset, GLsizeiptr size, const GLvoid *data) {
	GLuint index = getBufferIndex(target);

	if(index) {
		BufferPtr buff = buffers[index];
		if(buff && offset < buff->size) {
			GLbyte *p = (GLbyte *) buff->ptr;
			if(offset + size > buff->size) {
				size -= offset + size - buff->size;
			}
			if(data) {
				memcpy(p + offset, data, size);
			}
		}
	}
}

void OGLE::glMapBuffer(GLenum target, GLenum access) {
	glState["GL_MAPPED_BUFFER_TARGET"] = new Blob(target);

	if(GLuint index = getBufferIndex(target)) {
		BufferPtr buff = buffers[index];

		if(buff) {
			buff->mapAccess = access;
		}
	}
}

void OGLE::glMapBufferPost(GLvoid *retValue) {
	BlobPtr target = glState["GL_MAPPED_BUFFER_TARGET"];

	if(target) {
		if(GLuint index = getBufferIndex(target->toEnum())) {
			BufferPtr buff = buffers[index];

			if(buff) {
				buff->map = retValue;
			}
		}
	}

	glState["GL_MAPPED_BUFFER_TARGET"] = 0;
}

void OGLE::glUnmapBuffer(GLenum target) {

	if(GLuint index = getBufferIndex(target)) {
		BufferPtr buff = buffers[index];

		if(buff) {
			if(buff->map && buff->mapAccess != 0*GL_WRITE_ONLY) {
				memcpy(buff->ptr, buff->map, buff->size);
			}
			buff->map = 0;
			buff->mapAccess = 0;
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////
// OGLE utility functions
////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
// OGLE's Helper Functions
/////////////////////////////////////////////////////////////////////////////////


void OGLE::initFunctions() {

	extensionVBOSupported = false;

	float oglVersion = callBacks->GetGLVersion();
	if(oglVersion >= 1.5f ||
		callBacks->IsGLExtensionSupported("GL_ARB_vertex_buffer_object")) {
		extensionVBOSupported = true;

	    //Attempty to get the core version
		iglGetBufferSubData = (GLvoid (GLAPIENTRY *) (GLenum, GLint, GLsizeiptr, GLvoid *))callBacks->GetGLFunction("glGetBufferSubData");

		if(iglGetBufferSubData == NULL) {
		  //Attempt to get the ARB version instead
		  iglGetBufferSubData = (GLvoid (GLAPIENTRY *) (GLenum, GLint, GLsizeiptr, GLvoid *))callBacks->GetGLFunction("glGetBufferSubDataARB");

		  if(iglGetBufferSubData == NULL) {
			 //Unable to get the entry point, don't check for VBOs
			 extensionVBOSupported = false;
		  }
		}
	}

}




void OGLE::addSet(ElementSetPtr set) {

  if(!set) return;

  if(
	  (set->mode == GL_TRIANGLES && OGLE::config.polyTypesEnabled["TRIANGLES"])
	  || (set->mode == GL_TRIANGLE_STRIP && OGLE::config.polyTypesEnabled["TRIANGLE_STRIP"])
	  || (set->mode == GL_TRIANGLE_FAN && OGLE::config.polyTypesEnabled["TRIANGLE_FAN"])
	  || (set->mode == GL_QUADS && OGLE::config.polyTypesEnabled["QUADS"])
	  || (set->mode == GL_QUAD_STRIP && OGLE::config.polyTypesEnabled["QUAD_STRIP"])
	  || (set->mode == GL_POLYGON && OGLE::config.polyTypesEnabled["POLYGON"])
	  ) {

	  objFile->addSet(set);
	  // no need to store the ElementSets
	  //  sets.push_back(set);

  }


}

void OGLE::newSet(GLenum mode)
{
	VAOPtr vao = getVertexArray();
	if (!vao)
		return;

	// search for an attribute with size 3 and type GL_FLOAT,
	// assume it's vertex position
	for (size_t i = 0; i < vao->attrib_descs.size(); ++i) {
		AttribDesc &desc = vao->attrib_descs[i];
		if (desc.enabled && desc.buffer_index && desc.size == 3 && desc.type == GL_FLOAT && !desc.normalized) {
			BufferPtr buff = buffers[desc.buffer_index];
			if (buff) {
				currSet = new OGLE::ElementSet(mode);
				GLsizeiptr pointer = (GLsizeiptr)desc.pointer;
				while (pointer < buff->size) {
					GLfloat *data = (GLfloat*)((GLbyte*)buff->ptr + pointer);
					currSet->addElement(new OGLE::Element(new OGLE::Vertex(data, 3), 0, 0));
					pointer += desc.stride;
				}
				return;
			}
		}
	}

}

#define GLVoidPDeRef(TYPE, ARRAY, INDEX) (  *((TYPE *)(((char *)ARRAY) + INDEX * sizeof(TYPE)))  )

GLuint OGLE::derefIndexArray(GLenum type, const GLvoid *indices, int i) {
	GLuint index = 0;

	switch (type) {
		case GL_UNSIGNED_BYTE: index = GLVoidPDeRef(GLubyte, indices, i); break;
		case GL_UNSIGNED_SHORT:  index = GLVoidPDeRef(GLushort, indices, i); break;
		case GL_UNSIGNED_INT:  index = GLVoidPDeRef(GLuint, indices, i);break;
    }

	return index;

}

bool OGLE::isElementLocked(int i) {
  GLint lock_first;
  GLsizei lock_count;
  BlobPtr lfP, lcP;

  lfP =	glState["GL_LOCK_ARRAYS_FIRST"];
  lock_first = lfP ? lfP->toInt() : -1;

  lcP =	glState["GL_LOCK_ARRAYS_COUNT"];
  lock_count = lcP ? lcP->toSizeI() : 0;

  return (lock_first > -1 && lock_count > 0) && (i < lock_first || i > lock_first + lock_count);
}


GLuint OGLE::getBufferIndex(GLenum target) {
	BlobPtr index;

	switch(target) {
		case GL_ARRAY_BUFFER:
			index = glState["GL_ARRAY_BUFFER_INDEX"]; break;
		case GL_ELEMENT_ARRAY_BUFFER:
			index = glState["GL_ELEMENT_ARRAY_BUFFER_INDEX"]; break;
		case GL_COPY_WRITE_BUFFER:
			index = glState["GL_COPY_WRITE_BUFFER_INDEX"]; break;
	}

	return index ? index->toUInt() : 0;
}

GLuint OGLE::getVertexArrayIndex() {
	BlobPtr index = glState["GL_VERTEX_ARRAY_INDEX"];
	return index ? index->toUInt() : 0;
}

OGLE::VAOPtr OGLE::getVertexArray() {
	GLuint index = getVertexArrayIndex();
	if (index) {
		if (!vaos[index])
			vaos[index] = new VAO();
		return vaos[index];
	}
	return 0;
}

const GLbyte *OGLE::getBufferedArray(const GLbyte *array) {
	GLuint buffIndex = 0;
	GLuint offset = 0;
	BufferPtr buff;


	if(buffIndex = getBufferIndex(GL_ARRAY_BUFFER)) {
		BufferPtr buff = buffers[buffIndex];
		if(buff && buff->ptr) {
			offset = (GLuint)array;
			array = ((GLbyte *)buff->ptr) + offset;
		}
	}
	return array;
}

const GLvoid *OGLE::getBufferedIndices(const GLvoid *indices) {

	VAOPtr vao = getVertexArray();
	if (!vao || !vao->index_buffer_index)
		return 0;

	BufferPtr buff = buffers[vao->index_buffer_index];
	if (!buff || buff->ptr == 0)
		return 0;

	GLuint offset = (GLuint)indices;
	return ((GLbyte*)buff->ptr) + offset;
}


// Look to see if the program has set up Vertex or Element buffers
// Copy them into buffers we will read from.

void OGLE::checkBuffers() {
	GLenum targets[2] = {GL_ARRAY_BUFFER, GL_ELEMENT_ARRAY_BUFFER};

	if(!(extensionVBOSupported && iglGetBufferSubData)) {
		return;
	}

	for(int i = 0; i < 2; i++) {
		int index = getBufferIndex(targets[i]);

		BufferPtr bp = buffers[index];
		if(bp) {
		  iglGetBufferSubData(targets[i], 0, bp->size, bp->ptr);
		}
	}
}


GLsizei OGLE::glTypeSize(GLenum type) {
	GLsizei size;
	switch (type) {
		case GL_SHORT: size = sizeof(GLshort); break;
		case GL_INT: size = sizeof(GLint); break;
		case GL_FLOAT: size = sizeof(GLfloat); break;
		case GL_DOUBLE: size = sizeof(GLdouble); break;
	}
	return size;
}



//////////////////////////////////////////////////////////////////////////////////
// OGLE::ElementSet functions
//////////////////////////////////////////////////////////////////////////////////

OGLE::ElementSet::ElementSet(GLenum _mode) :
	mode(_mode)
{}

void OGLE::ElementSet::addElement(const GLfloat V[], GLsizei dim) {
	addElement(new Vertex(V, dim));
}

void OGLE::ElementSet::addElement(VertexPtr V) {
	addElement(new OGLE::Element(V));
}

void OGLE::ElementSet::addElement(ElementPtr E) {
	float scale = OGLE::config.scale;
	if(scale) {
		if(E->v) {
			E->v->x *= scale;
			E->v->y *= scale;
			E->v->z *= scale;
		}

		if(E->n) {
			E->n->x *= scale;
			E->n->y *= scale;
			E->n->z *= scale;
		}
	}

	elements.push_back(E);
}

