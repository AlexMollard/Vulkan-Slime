#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>

#include "rendering.h"
#include "swapChain.h"
#include "deviceAndQueue.h"
#include "image.h"
#include "vertex.h"

void rendering::draw(deviceAndQueue &deviceAndQueue, swapChain &swapChain, window &window, vertexInput &vertexBuffer,
                     image &image) {
    auto &device = deviceAndQueue.getDevice();

    vkWaitForFences(device, 1, &mInFlightFences[mCurrentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapChain.getSwapChain(), UINT64_MAX,
                                            mImageAvailableSemaphores[mCurrentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        swapChain.recreateSwapChain(deviceAndQueue, window, vertexBuffer, image);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw vulkanError("failed to acquire swap chain image!");
    }

    if (mImagesInFlight[imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device, 1, &mImagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
    }
    mImagesInFlight[imageIndex] = mInFlightFences[mCurrentFrame];

    // UNIFORMS
    updateUniform(imageIndex, device, swapChain, vertexBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    std::array<VkSemaphore, 1> waitSemaphores = {mImageAvailableSemaphores[mCurrentFrame]};
    std::array<VkPipelineStageFlags, 1> waitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores.data();
    submitInfo.pWaitDstStageMask = waitStages.data();

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &swapChain.getCommandBuffers()[imageIndex];

    std::array<VkSemaphore, 1> signalSemaphores = {mRenderFinishedSemaphores[mCurrentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores.data();

    vkResetFences(device, 1, &mInFlightFences[mCurrentFrame]);

    if (vkQueueSubmit(deviceAndQueue.getGraphicsQueue(), 1, &submitInfo, mInFlightFences[mCurrentFrame]) !=
        VK_SUCCESS) {
        throw vulkanError("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores.data();

    std::array<VkSwapchainKHR, 1> swapChains = {swapChain.getSwapChain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains.data();

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(deviceAndQueue.getPresentQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.getFrameBufferResized()) {
        window.setFrameBufferResized(false);
        swapChain.recreateSwapChain(deviceAndQueue, window, vertexBuffer, image);
    } else if (result != VK_SUCCESS) {
        throw vulkanError("failed to present swap chain image!");
    }

    mCurrentFrame = (mCurrentFrame + 1) % maxFramesInFlight;
}

void rendering::createSyncObjects(const VkDevice &device, swapChain &swapChain) {
    mImageAvailableSemaphores.resize(maxFramesInFlight);
    mRenderFinishedSemaphores.resize(maxFramesInFlight);
    mInFlightFences.resize(maxFramesInFlight);
    mImagesInFlight.resize(swapChain.getSwapChainImages().size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < maxFramesInFlight; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &mImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
            throw vulkanCreateError("Synchronization objects for a frame");
        }
    }
}

void rendering::cleanUp(VkDevice &device) {
    for (size_t i = 0; i < maxFramesInFlight; i++) {
        vkDestroySemaphore(device, mRenderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, mImageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, mInFlightFences[i], nullptr);
    }
}

void rendering::updateUniform(uint32_t currentImage, VkDevice &device, swapChain &swapChain, vertexInput &vertexInput) {
    static auto startTime = std::chrono::high_resolution_clock::now();

    float time = std::chrono::duration<float, std::chrono::seconds::period>(
            std::chrono::high_resolution_clock::now() - startTime).count();

    uniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj = glm::perspective(glm::radians(90.0f), (float) swapChain.getSwapChainExtent().width /
                                                     (float) swapChain.getSwapChainExtent().height, 0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    void *data;
    vkMapMemory(device, vertexInput.getUniformBuffersMemory()[currentImage], 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(device, vertexInput.getUniformBuffersMemory()[currentImage]);
}
