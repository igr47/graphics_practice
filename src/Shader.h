#ifndef SHADER_H
#define SHADER_H
#include <cstddef>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>

class Shader {
    public:
        int Program;
        // Constructor which genertes the shader on the fly
        Shader(const char* vertexPath, const char* fragmentPath) {
            std::string vertexCode;
            std::string fragmentCode;
            std::ifstream vShaderFile;
            std::ifstream fShaderFile;

            // Ensures ifstream objects can throw exeptions 
            vShaderFile.exceptions( std::ifstream::badbit);
            fShaderFile.exceptions( std::ifstream::badbit);

            try {
                // Open files 
                vShaderFile.open( vertexPath );
                fShaderFile.open( fragmentPath );

                std::stringstream vShaderStream, fShaderStream;
                // Read files buffer content into a stream 
                vShaderStream << vShaderFile.rdbuf();
                fShaderStream << fShaderFile.rdbuf();

                vShaderFile.close();
                fShaderFile.close();

                // Convert into string 
                vertexCode = vShaderStream.str();
                fragmentCode = fShaderStream.str();
            } catch (std::ifstream::failure e) {
                std::cout << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ" << std::endl;
            }

            const char *vShaderCode = vertexCode.c_str();
            const char *fShaderCode = fragmentCode.c_str();

            unsigned int vertex, fragment;
            int success;
            char infoLog[512];

            // VertexShader
            vertex = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertex, 1, &vShaderCode, NULL);
            glCompileShader(vertex);
            glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(vertex, 512, NULL, infoLog);
                std::cout << "VERTEX SHADER COMPILE ERROR: " << infoLog << std::endl
            }

            fragment = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragment, 1, &fShaderCode, NULL);
            glCompileShader(fragment);
            glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(fragment, 512, NULL, infoLog);
                std::cout << "FRAGMENT SHADER COMPILATION ERROR: " << infoLog << std::endl;
            }

            this->Program = glCreateProgram();
            glAttachShader(this->Program, vertex);
            glAttachShader(this->Program, fragment);
            glLinkProgram(this->Program);
            glGetProgramiv(this->Program, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(this->Program, 512, NULL, infoLog);
                std::cout << "SHADER LINKING ERROR: " << infoLog << std::endl;
            }

            glDeleteShader( vertex );
            glDeleteShader( fragment );
        }

        void User() {
            glUseProgram ( this->Program );
        }
};


#endif // !SHADER_H
#define SHADER_H
#include <cstddef>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <glad/glad.h>

class Shader {
    public:
        int Program;
        // Constructor which genertes the shader on the fly
        Shader(const char* vertexPath, const char* fragmentPath) {
            std::string vertexCode;
            std::string fragmentCode;
            std::ifstream vShaderFile;
            std::ifstream fShaderFile;

            // Ensures ifstream objects can throw exeptions 
            vShaderFile.exceptions( std::ifstream::badbit);
            fShaderFile.exceptions( std::ifstream::badbit);

            try {
                // Open files 
                vShaderFile.open( vertexPath );
                fShaderFile.open( fragmentPath );

                std::stringstream vShaderStream, fShaderStream;
                // Read files buffer content into a stream 
                vShaderStream << vShaderFile.rdbuf();
                fShaderStream << fShaderFile.rdbuf();

                vShaderFile.close();
                fShaderFile.close();

                // Convert into string 
                vertexCode = vShaderStream.str();
                fragmentCode = fShaderStream.str();
            } catch (std::ifstream::failure e) {
            
            }

            const char *vShaderCode = vertexCode.c_str();
            const char *fShaderCode = fragmentCode.c_str();

            unsigned int vertex, fragment;
            int success;
            char infoLog[512];

            // VertexShader
            vertex = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vertex, 1, &vShaderCode, NULL);
            glCompileShader(vertex);
            glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(vertex, 512, NULL, infoLog);
                std::cout << "VERTEX SHADER COMPILE ERROR: " << infoLog << std::endl
            }

            fragment = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fragment, 1, &fShaderCode, NULL);
            glCompileShader(fragment);
            glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
            if (!success) {
                glGetShaderInfoLog(fragment, 512, NULL, infoLog);
                std::cout << "FRAGMENT SHADER COMPILATION ERROR: " << infoLog << std::endl;
            }

            this->Program = glCreateProgram();
            glAttachShader(this->Program, vertex);
            glAttachShader(this->Program, fragment);
            glLinkProgram(this->Program);
            glGetProgramiv(this->Program, GL_LINK_STATUS, &success);
            if (!success) {
                glGetProgramInfoLog(this->Program, 512, NULL, infoLog);
                std::cout << "SHADER LINKING ERROR: " << infoLog << std::endl;
            }

            glDeleteShader( vertex );
            glDeleteShader( fragment );
        }

        void User() {
            glUseProgram ( this->Program );
        }
};

