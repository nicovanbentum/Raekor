#pragma once

inline void gThrowIfFailed(VkResult result) {
    if (result != VK_SUCCESS) {
        __debugbreak();
    }
}


inline VkTransformMatrixKHR gVkTransformMatrix(const glm::mat4& matrix) {
    glm::mat4 transpose = glm::transpose(matrix);

    VkTransformMatrixKHR result;
    memcpy(&result, glm::value_ptr(transpose), sizeof(VkTransformMatrixKHR));

    return result;
}
