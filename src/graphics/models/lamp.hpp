#ifndef LAMP_HPP
#define LAMP_HPP

#include "cube.hpp"
#include "../light.h"

//#include "../rendering/material.h"
#//include <glm/glm.hpp>

class Lamp : public Cube {
public:
    glm::vec3 lightColor;

    //-- light strength value ----
    PointLight pointLight;

    //-- default constructor ----
    Lamp()  {};

    Lamp(glm::vec3 lightColor,
        glm::vec4 ambient,
        glm::vec4 diffuse,
        glm::vec4 specular,
        float k0,
        float k1,
        float k2,
        glm::vec3 pos,
        glm::vec3 size)
        : lightColor(lightColor),
        pointLight({pos, k0, k1, k2, ambient, diffuse, specular}),
        Cube(pos, size) {}


    //-- set light color ------------
    void render(Shader shader) {
        shader.set3Float("lightColor", lightColor);

        Cube::render(shader);
    }
    /*
    Lamp(unsigned int maxNoInstances, glm::vec3 lightColor = glm::vec3(1.0f))
        : Cube(maxNoInstances, Material::white_rubber) {
        id = "lamp";
        this->lightColor = lightColor;
    }
    */
};

#endif