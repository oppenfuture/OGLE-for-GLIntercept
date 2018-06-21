#include "stdafx.h"
#include "../../MainLib/InterceptPluginInterface.h"
#include "OGLEPlugin.h"
#include "ogle.h"

#include <ConfigParser.h>
#include <CommonErrorLog.h>
#include <MiscUtils.h>

#include <algorithm>

#include "Ptr/Ptr.in"

USING_ERRORLOG

//Path to the dll
extern string dllPath;


#include <string>

//Include the common DLL code
#include <PluginCommon.cpp>

///////////////////////////////////////////////////////////////////////////////

InterceptPluginInterface * GLIAPI CreateFunctionLogPlugin(const char *pluginName, InterceptPluginCallbacks * callBacks)
{
  //If no call backs, return NULL
  if(callBacks == NULL)
  {
    return NULL;
  }

  //Assign the error logger
  if(errorLog == NULL)
  {
    errorLog = callBacks->GetLogErrorFunction();
  }

  string cmpPluginName(pluginName);

  //Test for the function status plugin
  if(cmpPluginName == "OGLE")
  {
    return new OGLEPlugin(callBacks);
  }

  return NULL;
}


///////////////////////////////////////////////////////////////////////////////

void OGLEPlugin::ProcessConfigData(ConfigParser *parser)
{
  //Get if logging by call count
  const ConfigToken *testToken = parser->GetToken("Scale");

  if(testToken)
  {
	  testToken->Get(OGLE::config.scale);
	  fprintf(OGLE::LOG, "SCALE: %f\n", OGLE::config.scale);
  }

  testToken = parser->GetToken("CaptureNormals");

  if(testToken)
  {
	  testToken->Get(OGLE::config.captureNormals);
	  fprintf(OGLE::LOG, "CAPTURE NORMALS: %d\n", OGLE::config.captureNormals);
  }

  testToken = parser->GetToken("CaptureTextureCoords");

  if(testToken)
  {
	  testToken->Get(OGLE::config.captureTexCoords);
	  fprintf(OGLE::LOG, "CAPTURE TEX COORDS: %d\n", OGLE::config.captureTexCoords);
  }


  testToken = parser->GetToken("FlipPolygonStrips");

  if(testToken)
  {
	  testToken->Get(OGLE::config.flipPolyStrips);
	  fprintf(OGLE::LOG, "FLIP POLY STRIPS: %d\n", OGLE::config.flipPolyStrips);
  }


  testToken = parser->GetToken("LogFunctions");

  if(testToken)
  {
	  testToken->Get(OGLE::config.logFunctions);
	  fprintf(OGLE::LOG, "LOG FUNCTIONS: %d\n", OGLE::config.logFunctions);
  }


  testToken = parser->GetToken("ObjFileName");

  if(testToken)
  {
	  testToken->Get(objFileName);
	  if(objFileName.find(".obj", objFileName.size() - 4) == (objFileName.size() - 4)) {
		  objFileName = objFileName.substr(0, objFileName.size() - 4);
	  }


	  fprintf(OGLE::LOG, "OBJFileName: %s\n", objFileName.c_str());
  }

  testToken = parser->GetToken("FilePerFrame");

  if(testToken)
  {
	  testToken->Get(filePerFrame);
	  fprintf(OGLE::LOG, "FilePerFrame: %d\n", filePerFrame);
  }

  testToken = parser->GetToken("FileInFrameDir");

  if(testToken)
  {
	  testToken->Get(fileInFrameDir);
	  fprintf(OGLE::LOG, "FileInFrameDir: %d\n", fileInFrameDir);
  }




  for(int i = 0; i < OGLE::Config::nPolyTypes; i++) {
	  const char *type = OGLE::Config::polyTypes[i];

	  testToken = parser->GetToken(type);

	  if(testToken)
	  {
		  bool tmp;
		  testToken->Get(tmp);
		  OGLE::config.polyTypesEnabled[type] = tmp;
		  fprintf(OGLE::LOG, "%s: %d\n", type, tmp);
	  }
  }

  fflush(OGLE::LOG);

}



///////////////////////////////////////////////////////////////////////////////
//
OGLEPlugin::OGLEPlugin(InterceptPluginCallbacks *callBacks):
gliCallBacks(callBacks),
GLV(callBacks->GetCoreGLFunctions()),
objFileName("ogle"),
filePerFrame(0),
fileInFrameDir(0),
isRecording(0)
{

/**/
  gliCallBacks->RegisterGLFunction("glBindVertexArray");
  gliCallBacks->RegisterGLFunction("glEnableVertexAttribArray");
  gliCallBacks->RegisterGLFunction("glDisableVertexAttribArray");
  gliCallBacks->RegisterGLFunction("glVertexAttribPointer");
  gliCallBacks->RegisterGLFunction("glVertexAttribIPointer");
  gliCallBacks->RegisterGLFunction("glVertexAttribLPointer");

  gliCallBacks->RegisterGLFunction("glDrawArrays");
  gliCallBacks->RegisterGLFunction("glDrawElements");

  gliCallBacks->RegisterGLFunction("glDrawRangeElements");
  gliCallBacks->RegisterGLFunction("glDrawRangeElementsEXT");
  gliCallBacks->RegisterGLFunction("glDrawElementsBaseVertex");

  gliCallBacks->RegisterGLFunction("glLockArraysEXT");
  gliCallBacks->RegisterGLFunction("glUnlockArraysEXT");

  gliCallBacks->RegisterGLFunction("glBindBuffer");
  gliCallBacks->RegisterGLFunction("glBindBufferARB");
  gliCallBacks->RegisterGLFunction("glBufferData");
  gliCallBacks->RegisterGLFunction("glBufferDataARB");
  gliCallBacks->RegisterGLFunction("glBufferStorage");
  gliCallBacks->RegisterGLFunction("glBufferSubData");
  gliCallBacks->RegisterGLFunction("glBufferSubDataARB");
  gliCallBacks->RegisterGLFunction("glMapBuffer");
  gliCallBacks->RegisterGLFunction("glMapBufferARB");
  gliCallBacks->RegisterGLFunction("glUnmapBuffer");
  gliCallBacks->RegisterGLFunction("glUnmapBufferARB");
/**/
  
  //Get calls that are even outside contexts  
  gliCallBacks->SetContextFunctionCalls(true);

  //Parse the config file
  ConfigParser fileParser;
  if(fileParser.Parse(dllPath + "config.ini"))
  {
    ProcessConfigData(&fileParser);
    fileParser.LogUnusedTokens(); 
  }

  //Parse the config string
  ConfigParser stringParser;
  if(stringParser.ParseString(gliCallBacks->GetConfigString()))
  {
    ProcessConfigData(&stringParser);
    stringParser.LogUnusedTokens(); 
  }

  ogle = new OGLE(callBacks, callBacks->GetCoreGLFunctions());
}


///////////////////////////////////////////////////////////////////////////////
//

void OGLEPlugin::Destroy()
{
  delete this;
}

OGLEPlugin::~OGLEPlugin()
{

}

///////////////////////////////////////////////////////////////////////////////
//
void OGLEPlugin::GLFunctionPre (uint updateID, const char *funcName, uint funcIndex, const FunctionArgs & args )
{

	//Create a access copy of the arguments
	FunctionArgs _args(args);

	if(isRecording || OGLE_BIND_BUFFERS_ALL_FRAMES) {
		// do these always, because sometimes data buffers get set up in earlier frames
		if(strcmp(funcName, "glBindBuffer") == 0 
			|| strcmp(funcName, "glBindBufferARB") == 0) {
			GLenum  target; _args.Get(target);
			GLuint  buffer; _args.Get(buffer);
			ogle->glBindBuffer(target , buffer);
		}
		else if(strcmp(funcName, "glBufferData") == 0
				|| strcmp(funcName, "glBufferDataARB") == 0) {
			GLenum  target; _args.Get(target);
			GLsizeiptr size; _args.Get(size);
			GLvoid * data; _args.Get(data);
			GLenum  usage; _args.Get(usage);
			ogle->glBufferData(target , size , data , usage);
		} else if (strcmp(funcName, "glBufferStorage") == 0) {
			GLenum target; _args.Get(target);
 			GLsizeiptr size; _args.Get(size);
 			const GLvoid * data; _args.Get(data);
 			GLbitfield flags; _args.Get(flags);
			ogle->glBufferStorage(target, size, data, flags);
		} else if (strcmp(funcName, "glBindVertexArray") == 0) {
			GLuint array; _args.Get(array);
			ogle->glBindVertexArray(array);
		} else if (strcmp(funcName, "glEnableVertexAttribArray") == 0) {
			GLuint index; _args.Get(index);
			ogle->glEnableVertexAttribArray(index);
		} else if (strcmp(funcName, "glDisableVertexAttribArray") == 0) {
			GLuint index; _args.Get(index);
			ogle->glDisableVertexAttribArray(index);
		} else if (strcmp(funcName, "glVertexAttribPointer") == 0
			|| strcmp(funcName, "glVertexAttribIPointer") == 0
			|| strcmp(funcName, "glVertexAttribLPointer") == 0) {
			GLuint index; _args.Get(index);
			GLint size; _args.Get(size);
			GLenum type; _args.Get(type);
			GLboolean normalized = false;
			if (strcmp(funcName, "glVertexAttribPointer") == 0)
				_args.Get(normalized);
			GLsizei stride; _args.Get(stride);
			const GLvoid *pointer; _args.Get(pointer);
			ogle->glVertexAttribPointer(index, size, type, normalized, stride, pointer);
		}




		if(OGLE_CAPTURE_BUFFERS_ALL_FRAMES) {
			// but we dont need to keep the actual data up-to-date, because we 
			// re-fetch before drawing.
			if(strcmp(funcName, "glBufferSubData") == 0
					|| strcmp(funcName, "glBufferSubDataARB") == 0) {
				GLenum  target; _args.Get(target);
				GLint offset; _args.Get(offset);
				GLsizeiptr  size; _args.Get(size);
				GLvoid * data; _args.Get(data);
				ogle->glBufferSubData(target , offset , size , data);
			}
			else if(strcmp(funcName, "glMapBuffer") == 0 
					|| strcmp(funcName, "glMapBufferARB") == 0) {
				GLenum  target; _args.Get(target);
				GLenum  access; _args.Get(access);
				ogle->glMapBuffer(target , access);
			}
			else if(strcmp(funcName, "glUnmapBuffer") == 0 
					|| strcmp(funcName, "glUnmapBufferARB") == 0) {
				GLenum  target; _args.Get(target);
				ogle->glUnmapBuffer(target);
			}
		}
	}

	if(!isRecording) return;
	
	if(OGLE::config.logFunctions) {
		char buff[1024];
		gliCallBacks->GetGLArgString(funcIndex, args, 1024, buff);
		fprintf(OGLE::LOG, "PRE FUNCTION (%d): %s\n", funcIndex, buff);
		fflush(OGLE::LOG);
	}

	if(strcmp(funcName, "glDrawElements") == 0) {
		GLenum mode; _args.Get(mode);
		GLsizei count; _args.Get(count);
		GLenum type; _args.Get(type);
		GLvoid *indices; _args.Get(indices);
		ogle->glDrawElements(mode , count , type , indices);
	}
	else if(strcmp(funcName, "glDrawArrays") == 0) {
		GLenum mode; _args.Get(mode);
		GLint first; _args.Get(first);
		GLsizei count; _args.Get(count);
		ogle->glDrawArrays(mode , first , count);
	}
	else if(strcmp(funcName, "glLockArraysEXT") == 0) {
		GLint first; _args.Get(first);
		GLsizei count; _args.Get(count);
		ogle->glLockArraysEXT(first, count);
	}
	else if(strcmp(funcName, "glUnlockArraysEXT") == 0) {
		ogle->glUnlockArraysEXT();
	}
	else if(strcmp(funcName, "glDrawRangeElements") == 0
			|| strcmp(funcName, "glDrawRangeElementsEXT") == 0
			) {
		GLenum  mode; _args.Get(mode);
		GLuint  start; _args.Get(start);
		GLuint  end; _args.Get(end);
		GLsizei  count; _args.Get(count);
		GLenum  type; _args.Get(type);
		GLvoid * indices; _args.Get(indices);
		ogle->glDrawRangeElements(mode , start , end , count , type , indices);
	} else if (strcmp(funcName, "glDrawElementsBaseVertex") == 0) {
		GLenum mode; _args.Get(mode);
 		GLsizei count; _args.Get(count);
 		GLenum type; _args.Get(type);
 		GLvoid *indices; _args.Get(indices);
 		GLint basevertex; _args.Get(basevertex);
		ogle->glDrawElementsBaseVertex(mode, count, type, indices, basevertex);
	}
	




	/*
	else if(strcmp(funcName, "gl") == 0) {
		ogle->gl();
	}
	*/

} 


///////////////////////////////////////////////////////////////////////////////
//
void OGLEPlugin::GLFunctionPost(uint updateID, const char *funcName, uint funcIndex, const FunctionRetValue & retVal)
{
	FunctionRetValue _retVal(retVal);


	if(isRecording || OGLE_CAPTURE_BUFFERS_ALL_FRAMES) {
		if(strcmp(funcName, "glMapBuffer") == 0 
				|| strcmp(funcName, "glMapBufferARB") == 0) {

			GLvoid *retValue; _retVal.Get(retValue);
			ogle->glMapBufferPost(retValue);
		}
	}


	if(!isRecording) return;
	
	if(OGLE::config.logFunctions) {
		char buff[1024];
		gliCallBacks->GetGLReturnString(funcIndex, retVal, 1024, buff);
		fprintf(OGLE::LOG, "\t%s (%d) returned: %s\n",funcName, funcIndex, buff);
		fflush(OGLE::LOG);
	}

}


///////////////////////////////////////////////////////////////////////////////
//
void OGLEPlugin::OnGLContextSet(HGLRC oldRCHandle, HGLRC newRCHandle)
{
//	fprintf(OGLE::LOG, "OP::OGLCS: %p, %p\n", oldRCHandle, newRCHandle);
	if(newRCHandle) {
		ogle->initFunctions();
	}
}


///////////////////////////////////////////////////////////////////////////////
//
void OGLEPlugin::GLFrameEndPost(const char *funcName, uint funcIndex, const FunctionRetValue & retVal)
{

	
	if(gliCallBacks->GetLoggerMode()) {
		fprintf(OGLE::LOG, "Starting to record, to filename %s\n", objFileName.c_str()); fflush(OGLE::LOG);
		isRecording = 1;
		string fileName = objFileName;
		unsigned int frame = gliCallBacks->GetFrameNumber();

		if(filePerFrame) {
			char buff[256];
			sprintf(buff, ".%d", gliCallBacks->GetFrameNumber());
			fileName.append(buff);
		}
		if(fileInFrameDir) {

			string path;
			StringPrintF(path,"Frame_%06u\\", frame);

			path.append(fileName);

			fileName = path;

			fprintf(OGLE::LOG, "frame file: %s\n", fileName.c_str() ); 
		}

		fileName.append(".obj");
		ogle->startRecording(fileName);
	}

#ifdef OGLE_GATHER_USELESS_FEEDBACK_BUFFER_DATA_TEST

	if(gliCallBacks->GetLoggerMode()) {
		fprintf(OGLE::LOG, "OP::GLFEPo: [%d] %s Logging: %d\n", gliCallBacks->GetFrameNumber(), funcName, gliCallBacks->GetLoggerMode()); 

		buff = new OGLE::Buffer(0, 1000000  * sizeof(GLfloat));
		fprintf(OGLE::LOG, "BUFFERING TO: %p\n", buff->ptr);
		GLV->glFeedbackBuffer(buff->size / sizeof(GLfloat), GL_3D, (GLfloat *)buff->ptr);
		GLV->glRenderMode(GL_FEEDBACK);
		fflush(OGLE::LOG);

		GLfloat *p = (GLfloat *)buff->ptr;
	}
#endif

}


///////////////////////////////////////////////////////////////////////////////
//
void OGLEPlugin::GLFrameEndPre(const char *funcName, uint funcIndex, const FunctionArgs & args )
{
	if(isRecording) {
		fprintf(OGLE::LOG, "Done recording\n"); fflush(OGLE::LOG);
		isRecording = 0;
		ogle->stopRecording();
	}


#ifdef OGLE_GATHER_USELESS_FEEDBACK_BUFFER_DATA_TEST

	if(gliCallBacks->GetLoggerMode()) {
		fprintf(OGLE::LOG, "OP::GLFEPr: [%d] %s Logging: %d\n", gliCallBacks->GetFrameNumber(), funcName, gliCallBacks->GetLoggerMode()); 

		int n = GLV->glRenderMode(GL_RENDER);
		fprintf(OGLE::LOG, "GL RENDER MODE RETURNED: %d\n", n);
		fflush(OGLE::LOG);
		if(n > 0) {
			FILE *DAT = fopen("ogle.dat", "w");
			fwrite(buff->ptr, 1, n * sizeof(GLfloat), DAT);
			fclose(DAT);

			GLfloat *p = (GLfloat *)buff->ptr;
			int i = 0;
			while(i < buff->size / sizeof(GLfloat)) {
				GLfloat tok = p[i++];
				if(tok == GL_POLYGON_TOKEN) {
					GLfloat n = p[i++];
					fprintf(OGLE::LOG, "GOT POLY WITH %f VERT [%f]\n", n, OGLE::scale); fflush(OGLE::LOG);
					OGLE::ElementSetPtr set = new OGLE::ElementSet(GL_POLYGON);
					for(int j = 0; j < n; j++) {
						OGLE::VertexPtr v = new OGLE::Vertex(p + i, 3);
						fprintf(OGLE::LOG, "[%f, %f, %f]\n", p[i], p[i+1], p[i+2]);

						set->addElement(v);
						i += 3;
					}
					set->printToObj(ogle->objFile);
					fflush(OGLE::LOG);
				}
			}

		}
	}
#endif

}

