//
// Created by alexmollard on 6/1/22.
//

#include "VulkanMesh.h"

//make sure that you are including the library
#define TINYOBJLOADER_IMPLEMENTATION

#include "tiny_obj_loader.h"
#include <iostream>

VertexInputDescription Vertex::get_vertex_description() {
    VertexInputDescription description;

    //we will have just 1 vertex buffer binding, with a per-vertex rate
    VkVertexInputBindingDescription mainBinding = {};
    mainBinding.binding = 0;
    mainBinding.stride = sizeof(Vertex);
    mainBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    description.bindings.push_back(mainBinding);

    //Position will be stored at Location 0
    VkVertexInputAttributeDescription positionAttribute = {};
    positionAttribute.binding = 0;
    positionAttribute.location = 0;
    positionAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    positionAttribute.offset = offsetof(Vertex, position);

    //Position will be stored at Location 0
    VkVertexInputAttributeDescription uvAttribute = {};
    uvAttribute.binding = 0;
    uvAttribute.location = 1;
    uvAttribute.format = VK_FORMAT_R32G32_SFLOAT;
    uvAttribute.offset = offsetof(Vertex, uv);

    //Normal will be stored at Location 1
    VkVertexInputAttributeDescription normalAttribute = {};
    normalAttribute.binding = 0;
    normalAttribute.location = 2;
    normalAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    normalAttribute.offset = offsetof(Vertex, normal);

    //Color will be stored at Location 2
    VkVertexInputAttributeDescription colorAttribute = {};
    colorAttribute.binding = 0;
    colorAttribute.location = 3;
    colorAttribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    colorAttribute.offset = offsetof(Vertex, color);

    description.attributes.push_back(positionAttribute);
    description.attributes.push_back(uvAttribute);
    description.attributes.push_back(normalAttribute);
    description.attributes.push_back(colorAttribute);
    return description;
}

bool Mesh::load_from_obj(const char *filename) {
    //attrib will contain the vertex arrays of the file
    tinyobj::attrib_t attrib;
    //shapes contains the info for each separate object in the file
    std::vector<tinyobj::shape_t> shapes;
    //materials contains the information about the material of each shape, but we won't use it.
    std::vector<tinyobj::material_t> materials;

    //error and warning output from the load function
    std::string warn;
    std::string err;

    //load the OBJ file
    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename, nullptr);
    //make sure to output the warnings to the console, in case there are issues with the file
    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }
    //if we have any error, print it to the console, and break the mesh loading.
    //This happens if the file can't be found or is malformed
    if (!err.empty()) {
        std::cerr << err << std::endl;
        return false;
    }

    // Loop over shapes
    for (auto &shape: shapes) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {

            //hardcode loading to triangles
            int fv = 3;

            // Loop over vertices in the face.
            for (size_t v = 0; v < fv; v++) {
                Vertex new_vert{};

                // access to vertex
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                //vertex position
                new_vert.position.x = attrib.vertices[3 * idx.vertex_index + 0];
                new_vert.position.y = attrib.vertices[3 * idx.vertex_index + 1];
                new_vert.position.z = attrib.vertices[3 * idx.vertex_index + 2];

                //vertex normal
                new_vert.normal.x = attrib.normals[3 * idx.normal_index + 0];
                new_vert.normal.y = attrib.normals[3 * idx.normal_index + 1];
                new_vert.normal.z = attrib.normals[3 * idx.normal_index + 2];

                //vertex uvs
                //new_vert.uv.x = attrib.texcoords[2 * idx.texcoord_index + 0];
                //new_vert.uv.y = attrib.texcoords[2 * idx.texcoord_index + 1];
                new_vert.uv.x = 0.0f;
                new_vert.uv.y = 0.0f;

                if (!attrib.colors.empty()) {
                    //vertex colours
                    new_vert.color.x = attrib.colors[3 * idx.vertex_index + 0];
                    new_vert.color.y = attrib.colors[3 * idx.vertex_index + 1];
                    new_vert.color.z = attrib.colors[3 * idx.vertex_index + 2];
                } else {
                    new_vert.color.x = 1.0f;
                    new_vert.color.y = 1.0f;
                    new_vert.color.z = 1.0f;
                }

                mVertices.push_back(new_vert);
            }
            index_offset += fv;
        }
    }

    return true;
}
