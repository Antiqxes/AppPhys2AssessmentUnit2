#pragma once

#include<glad/glad.h>
#include <GLFW/glfw3.h>
#include<iostream>
#include<cmath>
#include<vector>

namespace {
	struct Vector3 {
		float x, y, z;
	};

	struct Mesh {
		GLuint VAO, VBO;
		int vertexCount;
	};

	struct Point {
		Mesh mesh;
		Vector3 position;
		Vector3 color;
		float radius;

		Point(float x, float y, float z, float r = 1.0f, int segments = 32)
			: mesh{ 0, 0, 0 },
			position{ x, y, z },
			color{ 1.0f, 1.0f, 1.0f },
			radius{ r }
		{
			// 1. Calculate vertex data
			std::vector<float> vertices;

			// Center point of the circle
			vertices.push_back(0.0f); // X
			vertices.push_back(0.0f); // Y
			vertices.push_back(0.0f); // Z

			// Outer edge points
			for (int i = 0; i <= segments; ++i) {
				float angle = i * 2.0f * 3.14159265f / segments;
				vertices.push_back(radius * std::cos(angle)); // X
				vertices.push_back(radius * std::sin(angle)); // Y
				vertices.push_back(0.0f);                     // Z
			}

			mesh.vertexCount = vertices.size() / 3;

			// 2. Generate and bind OpenGL buffers
			glGenVertexArrays(1, &mesh.VAO);
			glGenBuffers(1, &mesh.VBO); // Fixed from previous step: glGenBuffers for VBO

			glBindVertexArray(mesh.VAO);

			glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
			glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

			// 3. Define vertex layout attributes
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
			glEnableVertexAttribArray(0);

			// Unbind to prevent accidental modifications elsewhere
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindVertexArray(0);
		}
	};
}

//OpenGL stuff
namespace {
	void setOpenGLContextVersionJ(int major, int minor) {
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, major);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minor);
		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	}

	void initializeGLADJ() {
		if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
			std::cout << "Failed to initialize GLAD" << std::endl;
			glfwTerminate();
			exit(-1);
		}
	}

	void processInput(GLFWwindow* window) {
		// GLFW automatically populates 'key' and 'action' based on what the user pressed
		if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
			glfwSetWindowShouldClose(window, true);
		}
	}
}
