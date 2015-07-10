/*
 * gl2ShaderProgram.cpp
 *
 *  Created on: 5 juin 2015
 *      Author: Nicolas
 */

#include "gl2Shaders.h"
#include "glog.h"

GLint ogl2ShaderProgram::curProg = -1;
ShaderProgram *ogl2ShaderProgram::current = NULL;

const char *hdrShaderCode=
#ifdef OPENGL_ES
    "#version 100\n"
    "#define GLES2\n";
#else
    "#version 120\n"
    "#define highp\n"
    "#define mediump\n"
    "#define lowp\n";
#endif


GLuint ogl2LoadShader(GLuint type, const char *hdr, const char *code, std::string &log) {
	GLuint shader = glCreateShader(type);
	const char *lines[2] = { hdr, code };
	glShaderSource(shader, 2, lines, NULL);
	glCompileShader(shader);

	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if (isCompiled == GL_FALSE) {
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		if (maxLength > 0) {
			//The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
			log.append((type==GL_FRAGMENT_SHADER)?"FragmentShader:\n":"VertexShader:\n");
			log.append(&infoLog[0]);
			log.append("\n");
			glog_e("Shader Compile: %s\n", &infoLog[0]);
		}
		glDeleteShader(shader);
		shader = 0;
	}
	glog_i("Loaded shader:%d\n", shader);
	return shader;
}

GLuint ogl2BuildProgram(GLuint vertexShader, GLuint fragmentShader, std::string log) {
	GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glBindAttribLocation(program, 0, "vVertex"); //Ensure vertex is at 0
	glLinkProgram(program);

	GLint maxLength = 0;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
	if (maxLength > 0) {
		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
		log.append("Shader Program:\n");
		log.append(&infoLog[0]);
		log.append("\n");
		glog_i("GL Program log:%s\n", &infoLog[0]);
	}
	glog_i("Loaded program:%d\n", program);
	return program;
}

bool ogl2ShaderProgram::isValid()
{
	return vertexShader&&fragmentShader&&program;
}

const char *ogl2ShaderProgram::compilationLog()
{
	return errorLog.c_str();
}

void ogl2ShaderProgram::deactivate() {
	for (std::vector<GLint>::iterator it = attributes.begin();
			it != attributes.end(); ++it) {
		GLint att = *it;
		if (att >= 0)
			glDisableVertexAttribArray(*it);
	}
	current = NULL;
}

void ogl2ShaderProgram::activate() {
	useProgram();
	if (current == this)
		return;
	if (current)
		current->deactivate();
	current = this;
	for (std::vector<GLint>::iterator it = attributes.begin();
			it != attributes.end(); ++it) {
		GLint att = *it;
		if (att >= 0)
			glEnableVertexAttribArray(*it);
	}

}

void ogl2ShaderProgram::useProgram() {
	if (curProg != program) {
		glUseProgram(program);
		curProg = program;
	}
}

void ogl2ShaderProgram::setData(int index, DataType type, int mult,
		const void *ptr, unsigned int count, bool modified,
		BufferCache **cache) {
	useProgram();
	GLenum gltype = GL_FLOAT;
	bool normalize = false;
	switch (type) {
	case DINT:
		gltype = GL_INT;
		break;
	case DBYTE:
		gltype = GL_BYTE;
		break;
	case DUBYTE:
		gltype = GL_UNSIGNED_BYTE;
		normalize = true; //TODO check vs actual shader type to see if normalization is required
		break;
	case DSHORT:
		gltype = GL_SHORT;
		break;
	case DUSHORT:
		gltype = GL_UNSIGNED_SHORT;
		break;
	case DFLOAT:
		gltype = GL_FLOAT;
		break;
	}
#ifdef GIDEROS_GL1
	glVertexPointer(mult,gltype, 0,ptr);
#else
	glVertexAttribPointer(attributes[index], mult, gltype, normalize, 0, ptr
#ifdef DXCOMPAT_H
			,count,modified,(GLuint *)cache
#endif
			);
#endif

}

void ogl2ShaderProgram::setConstant(int index, ConstantType type, int mult,
		const void *ptr) {
	if ((!updateConstant(index, type, mult, ptr))&&(!(uninit_uniforms&(1<<index))))
		return;
	useProgram();
	uninit_uniforms&=~(1<<index);
	switch (type) {
	case CINT:
	case CTEXTURE:
		glUniform1iv(gluniforms[index], mult,((GLint *) ptr));
		break;
	case CFLOAT:
		glUniform1fv(gluniforms[index],mult, ((GLfloat *) ptr));
		break;
	case CFLOAT4:
		glUniform4fv(gluniforms[index], mult, ((GLfloat *) ptr));
		break;
	case CMATRIX:
		glUniformMatrix4fv(gluniforms[index], mult, false, ((GLfloat *) ptr));
		break;
	}
	/*
#ifdef GIDEROS_GL1
	glColor4f(r,g,b,a);
#endif
*/
}

ogl2ShaderProgram::ogl2ShaderProgram(const char *vshader, const char *fshader,int flags,
		const ConstantDesc *uniforms, const DataDesc *attributes) {
	void *vs = LoadShaderFile(vshader, "glsl", NULL);
	void *fs = LoadShaderFile(fshader, "glsl", NULL);
	const char *hdr=(flags&ShaderProgram::Flag_NoDefaultHeader)?"":hdrShaderCode;
	buildProgram(hdr,(char *) vs, hdr, (char *) fs, uniforms, attributes);
}

ogl2ShaderProgram::ogl2ShaderProgram(const char *vshader1, const char *vshader2,
		const char *fshader1, const char *fshader2,
		const ConstantDesc *uniforms, const DataDesc *attributes) {
	buildProgram(vshader1, vshader2, fshader1, fshader2, uniforms, attributes);
}

void ogl2ShaderProgram::buildProgram(const char *vshader1, const char *vshader2,
		const char *fshader1, const char *fshader2,
		const ConstantDesc *uniforms, const DataDesc *attributes) {
	cbsData=0;
	errorLog="";
    uninit_uniforms=-1;
	vertexShader = ogl2LoadShader(GL_VERTEX_SHADER, vshader1, vshader2,errorLog);
	fragmentShader = ogl2LoadShader(GL_FRAGMENT_SHADER, fshader1, fshader2,errorLog);
	program = ogl2BuildProgram(vertexShader, fragmentShader,errorLog);
	glUseProgram(program);
	GLint ntex = 0;
	while (uniforms->name) {
		int usz = 0, ual = 4;
		ConstantDesc cd;
		cd = *(uniforms++);
		this->gluniforms.push_back(glGetUniformLocation(program, cd.name));
		switch (cd.type) {
		case CTEXTURE:
			usz = 4;
			ual = 4;
			glUniform1i(gluniforms[gluniforms.size() - 1], ntex++);
			break;
		case CINT:
			usz = 4;
			ual = 4;
			break;
		case CFLOAT:
			usz = 4;
			ual = 4;
			break;
		case CFLOAT4:
			usz = 16;
			ual = 16;
			break;
		case CMATRIX:
			usz = 64;
			ual = 16;
			break;
		}
		if (cbsData & (ual - 1))
			cbsData += ual - (cbsData & (ual - 1));
		cd.offset = cbsData;
		cbsData += usz*cd.mult;
		this->uniforms.push_back(cd);
	}
	cbData = malloc(cbsData);
	for (int iu = 0; iu < this->uniforms.size(); iu++)
		this->uniforms[iu]._localPtr = ((char *) cbData)
				+ this->uniforms[iu].offset;

	while (attributes->name) {
		this->attributes.push_back(
				glGetAttribLocation(program, attributes->name));
		attributes++;
	}
	shaderInitialized();
}

ogl2ShaderProgram::~ogl2ShaderProgram() {
	if (current==this)
		deactivate();
	if (curProg == program) {
		glUseProgram(0);
		curProg = 0;
	}
	glDetachShader(program, vertexShader);
	glDetachShader(program, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	glDeleteProgram(program);
	glog_i("Deleted program:%d\n", program);
	free(cbData);
}

void ogl2ShaderProgram::drawArrays(ShapeType shape, int first,
		unsigned int count) {
	ShaderEngine::Engine->prepareDraw(this);
	activate();
	GLenum mode = GL_POINTS;
	switch (shape) {
	case Point:
		mode = GL_POINTS;
		break;
	case Lines:
		mode = GL_LINES;
		break;
	case LineLoop:
		mode = GL_LINE_LOOP;
		break;
	case Triangles:
		mode = GL_TRIANGLES;
		break;
	case TriangleFan:
		mode = GL_TRIANGLE_FAN;
		break;
	case TriangleStrip:
		mode = GL_TRIANGLE_STRIP;
		break;
	}
	glDrawArrays(mode, first, count);

}
void ogl2ShaderProgram::drawElements(ShapeType shape, unsigned int count,
		DataType type, const void *indices, bool modified, BufferCache *cache) {
	ShaderEngine::Engine->prepareDraw(this);
	activate();

	GLenum mode = GL_POINTS;
	switch (shape) {
	case Point:
		mode = GL_POINTS;
		break;
	case Lines:
		mode = GL_LINES;
		break;
	case LineLoop:
		mode = GL_LINE_LOOP;
		break;
	case Triangles:
		mode = GL_TRIANGLES;
		break;
	case TriangleFan:
		mode = GL_TRIANGLE_FAN;
		break;
	case TriangleStrip:
		mode = GL_TRIANGLE_STRIP;
		break;
	}

	GLenum dtype = GL_INT;
	switch (type) {
	case DFLOAT:
		dtype = GL_FLOAT;
		break;
	case DBYTE:
		dtype = GL_BYTE;
		break;
	case DUBYTE:
		dtype = GL_UNSIGNED_BYTE;
		break;
	case DSHORT:
		dtype = GL_SHORT;
		break;
	case DUSHORT:
		dtype = GL_UNSIGNED_SHORT;
		break;
	case DINT:
		dtype = GL_INT;
		break;
	}
	glDrawElements(mode, count, dtype, indices
#ifdef DXCOMPAT_H
			, modified, (GLuint *)cache
#endif
			);
}