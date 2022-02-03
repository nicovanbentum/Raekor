#pragma once

inline void ThrowIfFailed(VkResult result) {
    if (result != VK_SUCCESS) {
        __debugbreak();
    }
}


inline VkTransformMatrixKHR VkTransformMatrix(const glm::mat4& matrix) {
    glm::mat4 transpose = glm::transpose(matrix);

    VkTransformMatrixKHR result;
    memcpy(&result, glm::value_ptr(transpose), sizeof(VkTransformMatrixKHR));

    return result;
}
