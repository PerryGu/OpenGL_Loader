#include "shader.h"


//--  default --------
Shader::Shader() {}

//-- initialize with paths to vertex and fragment shaders ----------
Shader::Shader(const char* vertexShaderPath, const char* fragShaderPath) {
	generate( vertexShaderPath, fragShaderPath);
}


void Shader::generate(const char* vertexShaderPath, const char* fragmentShaderPath){
	int success;
	char infoLog[512];

	GLuint vertexShader = compileShader(vertexShaderPath, GL_VERTEX_SHADER);
	GLuint fragShader = compileShader(fragmentShaderPath, GL_FRAGMENT_SHADER);

	id = glCreateProgram();
	glAttachShader(id, vertexShader);
	glAttachShader(id, fragShader);
	glLinkProgram(id);

	//-- catch Program shader error -------------------------
	glGetProgramiv(id, GL_LINK_STATUS, &success);
	if (!success) {
		glGetProgramInfoLog(id, 512, NULL, infoLog);
		std::cout << "Linking error:" << std::endl << infoLog << std::endl;
	}

	//-- delete shaders ------------------------
	glDeleteShader(vertexShader);
	glDeleteShader(fragShader);

}

//-- activate Shader ------------------------
void Shader::activate()
{
	glUseProgram(id);
}

//-- load Shader file -----------------
std::string Shader::loadShaderSrc(const char* filename)
{
	std::ifstream file;
	std::stringstream buf;

	std::string ret = "";

	file.open(filename);
	if (file.is_open())
	{
		buf << file.rdbuf();
		ret = buf.str();
	}
	else
	{
		std::cout << "Could not open " << filename << std::endl;
	}

	file.close();

	return  ret;

}

//-- compile the Shader --------------------
GLuint Shader::compileShader(const char* filepath, GLenum type)
{
	int succsess;
	char infoLog[512];

	GLuint ret = glCreateShader(type);
	std::string shaderSrc = loadShaderSrc(filepath);
	const GLchar* shader = shaderSrc.c_str();
	glShaderSource(ret, 1, &shader, NULL);
	glCompileShader(ret);

	//-- catch Program shader error -------------------------
	glGetShaderiv(ret, GL_COMPILE_STATUS, &succsess);
	if (!succsess) {
		glGetProgramInfoLog(ret, 512, NULL, infoLog);
		std::cout << "Error with fragment shader comp.:" << std::endl << infoLog << std::endl;
	}

	return ret;

}


void Shader::setBool(const std::string& name, bool value) {
	glUniform1i(glGetUniformLocation(id, name.c_str()), (int)value);
}

void Shader::setInt(const std::string& name, int value) {
	glUniform1i(glGetUniformLocation(id, name.c_str()), value);
}

void Shader::setFloat(const std::string& name, float value) {
	glUniform1f(glGetUniformLocation(id, name.c_str()), value);
}

void Shader::set3Float(const std::string& name, glm::vec3 v) {
	set3Float(name,  v.x, v.y, v.z);
	//glUniform3f(glGetUniformLocation(id, name.c_str()), v.x, v.y, v.z);
}

void Shader::set3Float(const std::string& name, float v1, float v2, float v3) {
	glUniform3f(glGetUniformLocation(id, name.c_str()), v1, v2, v3);
}

void Shader::set4Float(const std::string& name, float v1, float v2, float v3, float v4) {
	glUniform4f(glGetUniformLocation(id, name.c_str()), v1, v2, v3, v4);
}

void Shader::set4Float(const std::string& name, aiColor4D color) {
	glUniform4f(glGetUniformLocation(id, name.c_str()), color.r, color.g, color.b, color.a);
}

void Shader::set4Float(const std::string& name, glm::vec4 v) {
	glUniform4f(glGetUniformLocation(id, name.c_str()), v.x, v.y, v.z, v.w);
}

//-- create matrix shader ---------------
void Shader::setMat4(const std::string& name, glm::mat4 val)
{
	glUniformMatrix4fv(glGetUniformLocation(id, name.c_str()), 1, GL_FALSE, glm::value_ptr(val));
}

//-- create bone_transforms ---------------
void Shader::setMat4(const std::string& name, int boneCount,  glm::mat4& val)
{
	GLint location = glGetUniformLocation(id, name.c_str());
	if (location == -1) {
		std::cout << "[Shader] WARNING: Uniform '" << name << "' not found in shader!" << std::endl;
		return;
	}
	glUniformMatrix4fv(location, boneCount, GL_FALSE, glm::value_ptr(val));
}