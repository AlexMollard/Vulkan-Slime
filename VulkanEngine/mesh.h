//
// Created by alexm on 21/12/2021.
//

#pragma once

#include <string>
#include "vertex.h"
#include "deviceAndQueue.h"

struct meshData {
    size_t startVertIndex;
    size_t startIndex;
    size_t numIndices;
};

class mesh {
public:
    explicit mesh();

    void init(deviceAndQueue &devices, VkCommandPool &commandPool, const std::string &inputfile);

    void cleanUp(VkDevice &device);

    meshData mMeshData{};
    std::vector<vertex> mVertexs;
    std::vector<uint32_t> mIndices;

    VkBuffer mVertexBuffer{};
    VkDeviceMemory mVertexBufferMemory{};

    VkBuffer mIndexBuffer{};
    VkDeviceMemory mIndexBufferMemory{};
};