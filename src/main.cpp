#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <cstring>

#include "vk_context.h"
#include "vk_helpers.h"
#include "hdr_loader.h"
#include "obj_loader.h"

namespace fs = std::filesystem;

// camera globals
static glm::vec3 cameraPos = glm::vec3(0.0f, 100.0f, 5.0f);
static glm::vec3 cameraFront = glm::normalize(glm::vec3(0.3f, 0.0f, 1.0f));
static glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

static float pitch = glm::degrees(asinf(cameraFront.y));
static float yaw = glm::degrees(atan2f(cameraFront.z, cameraFront.x));
static float lastX = 400, lastY = 300;
static bool firstMouse = true;
static float fov = 45.0f;

static float deltaTime = 0.0f;
static float lastFrame = 0.0f;

// these values are just what i felt looked best on startup but they can be adjusted during runtime
static float gWaveSpeed = 3.00f;

static float gHeightScale = 100.0f;
static float gChoppy = 6.0f;
static float gSwellAmp = 1.5f;
static float gSwellSpeed = 0.25f;

static float dayNight = 1.0f;
static bool wireframe = false;
static int shaderDebug = 0;

static bool infiniteOcean = true;
static int oceanRadius = 12;
static int lod0Radius = 2;
static int lod1Radius = 5;

static float cameraSpeedBase = 10.0f;

static glm::vec2 worldOrigin(0.0f, 0.0f);

// boat stuffs
static bool gBoatEnabled = true;
static glm::vec2 gBoatPos = glm::vec2(0.0f, 0.0f);
static float gBoatYaw = 0.0f;
static float gBoatSpeed = 0.0f;
static glm::vec2 gBoatVel = glm::vec2(0.0f, 0.0f);

static float gBoatLen = 3.0f;
static float gBoatWid = 2.2f;
static float gBoatH = 2.0f;
static float gDuckScaleMeters = 8.0f;
static float gBoatDraft = 0.25f;

// i added this for duckl motion its kinda fun but its still not tweaked for "Realism"
static float gDuckSpeed = 35.0f;
static float gDuckAccel = 2.5f;
static float gDuckTurnRate = 1.6f;
static float gDuckDrag = 1.8f;
static float gDuckThrottle = 0.0f;
static float gDuckEdgeMargin = 10.0f;

static float gExposure = 0.55f;
static float gBloomStrength = 0.85f;

struct MeshVert
{
    float xz[2];
    float uv[2];
    float skirt;
};

static void printWaveParams()
{
    std::cout << "Wave params: height=" << gHeightScale
              << "  choppy=" << gChoppy
              << "  swellAmp=" << gSwellAmp
              << "  swellSpeed=" << gSwellSpeed
              << "  exposure=" << gExposure
              << "  waveSpeed=" << gWaveSpeed
              << "  bloom=" << gBloomStrength
              << "\n";
}

static void mouse_callback(GLFWwindow *window, double xpos, double ypos)
{
    (void)window;
    float sensitivity = 0.1f;

    if (firstMouse)
    {
        lastX = (float)xpos;
        lastY = (float)ypos;
        firstMouse = false;
    }

    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos;
    lastY = (float)ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;
    pitch = std::clamp(pitch, -89.0f, 89.0f);

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

static void processInput(GLFWwindow *window, float dt)
{
    float speed = cameraSpeedBase * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        speed *= 3.0f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= speed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;

    static bool nPressed = true;
    if (glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS)
    {
        if (!nPressed)
        {
            dayNight = 1.0f - dayNight;
            nPressed = true;
        }
    }
    else
        nPressed = false;

    static bool mPressed = false;
    if (glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS)
    {
        if (!mPressed)
        {
            wireframe = !wireframe;
            mPressed = true;
        }
    }
    else
        mPressed = false;

    static bool pPressed = false;
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        if (!pPressed)
        {
            infiniteOcean = !infiniteOcean;
            pPressed = true;
        }
    }
    else
        pPressed = false;

    static bool d0 = false, d1 = false, d2 = false;
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
    {
        if (!d0)
        {
            shaderDebug = 0;
            d0 = true;
        }
    }
    else
        d0 = false;
    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
    {
        if (!d1)
        {
            shaderDebug = 1;
            d1 = true;
        }
    }
    else
        d1 = false;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
    {
        if (!d2)
        {
            shaderDebug = 2;
            d2 = true;
        }
    }
    else
        d2 = false;

    static float keyRepeat = 0.0f;
    keyRepeat -= dt;
    if (keyRepeat <= 0.0f)
    {
        bool changed = false;

        if (glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
        {
            gHeightScale = std::max(0.0f, gHeightScale - 0.5f);
            changed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
        {
            gHeightScale = std::min(200.0f, gHeightScale + 0.5f);
            changed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS)
        {
            gChoppy = std::max(0.0f, gChoppy - 0.25f);
            changed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS)
        {
            gChoppy = std::min(50.0f, gChoppy + 0.25f);
            changed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_SEMICOLON) == GLFW_PRESS)
        {
            gSwellAmp = std::max(0.0f, gSwellAmp - 0.1f);
            changed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_APOSTROPHE) == GLFW_PRESS)
        {
            gSwellAmp = std::min(20.0f, gSwellAmp + 0.1f);
            changed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
        {
            gSwellSpeed = std::max(0.0f, gSwellSpeed - 0.02f);
            changed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
        {
            gSwellSpeed = std::min(5.0f, gSwellSpeed + 0.02f);
            changed = true;
        }

        if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
        {
            gWaveSpeed = std::max(0.05f, gWaveSpeed - 0.05f);
            changed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
        {
            gWaveSpeed = std::min(5.00f, gWaveSpeed + 0.05f);
            changed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS)
        {
            gExposure = std::min(2.0f, gExposure + 0.03f);
            changed = true;
        }
        if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS)
        {
            gExposure = std::max(0.2f, gExposure - 0.03f);
            changed = true;
        }

        if (changed)
        {
            printWaveParams();
            keyRepeat = 0.05f;
        }
    }

    // duck controls
    if (gBoatEnabled)
    {
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            gDuckThrottle = std::min(1.0f, gDuckThrottle + gDuckAccel * dt);
        else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            gDuckThrottle = std::max(-1.0f, gDuckThrottle - gDuckAccel * dt);
        else
        {
            float k = std::exp(-gDuckDrag * dt);
            gDuckThrottle *= k;
        }

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            gBoatYaw += gDuckTurnRate * dt;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            gBoatYaw -= gDuckTurnRate * dt;

        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            gBoatYaw += 1.0f * dt;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            gBoatYaw -= 1.0f * dt;

        glm::vec2 fwd(std::cos(gBoatYaw), std::sin(gBoatYaw));

        gBoatSpeed = gDuckThrottle * gDuckSpeed;
        gBoatVel = fwd * gBoatSpeed;
        gBoatPos += gBoatVel * dt;

        // keep him trapped
        if (!infiniteOcean)
        {
            float half = 0.5f * 512.0f - gDuckEdgeMargin;
            glm::vec2 minW = worldOrigin + glm::vec2(-half, -half);
            glm::vec2 maxW = worldOrigin + glm::vec2(half, half);
            gBoatPos = glm::clamp(gBoatPos, minW, maxW);
        }

        // reset
        static bool bPressed = false;
        if (glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS)
        {
            if (!bPressed)
            {
                glm::vec2 camW = worldOrigin + glm::vec2(cameraPos.x, cameraPos.z);
                gBoatPos = camW + fwd * 20.0f;
                if (!infiniteOcean)
                {
                    float half = 0.5f * 512.0f - gDuckEdgeMargin;
                    glm::vec2 minW = worldOrigin + glm::vec2(-half, -half);
                    glm::vec2 maxW = worldOrigin + glm::vec2(half, half);
                    gBoatPos = glm::clamp(gBoatPos, minW, maxW);
                }
                gBoatSpeed = 0.0f;
                gDuckThrottle = 0.0f;
                bPressed = true;
            }
        }
        else
            bPressed = false;
    }
}

static constexpr int FREQ_SIZE = 256;
static constexpr float PATCH_SIZE = 512.0f;

static constexpr uint32_t MAX_PARTICLES = 16384;

struct alignas(16) GlobalUBO
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos_time;
    glm::vec4 wave0;
    glm::vec4 worldOrigin_pad;
    glm::vec4 wave1;
    glm::ivec4 debug;
    glm::vec4 screen;
    glm::vec4 boat0;
    glm::vec4 boat1;
};

struct alignas(16) TaaUBO
{
    glm::mat4 invCurrVP;
    glm::mat4 prevVP;
    glm::vec4 params;
};

struct alignas(16) WaterPush
{
    glm::vec2 worldOffset;
    glm::vec2 _pad;
};

struct alignas(16) BoatPush
{
    glm::vec4 boat0;
    glm::vec4 boat1;
};

static VkPipeline createGraphicsPipeline(
    VkDevice device,
    VkRenderPass renderPass,
    VkPipelineLayout layout,
    VkExtent2D extent,
    const std::string &vsPath,
    const std::string &fsPath,
    bool enableVertexInput,
    bool depthWrite,
    VkCompareOp depthCompare,
    VkPolygonMode polyMode,
    VkCullModeFlags cullMode,
    bool depthTest = true,
    bool enableBlend = false)
{
    auto vsCode = readFileBinary(vsPath);
    auto fsCode = readFileBinary(fsPath);
    VkShaderModule vs = createShaderModule(device, vsCode);
    VkShaderModule fs = createShaderModule(device, fsCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VkVertexInputBindingDescription bind{};
    std::array<VkVertexInputAttributeDescription, 3> attrs{};
    if (enableVertexInput)
    {
        bind.binding = 0;
        bind.stride = sizeof(MeshVert);
        bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        attrs[0].location = 0;
        attrs[0].binding = 0;
        attrs[0].format = VK_FORMAT_R32G32_SFLOAT;
        attrs[0].offset = offsetof(MeshVert, xz);

        attrs[1].location = 1;
        attrs[1].binding = 0;
        attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset = offsetof(MeshVert, uv);

        attrs[2].location = 2;
        attrs[2].binding = 0;
        attrs[2].format = VK_FORMAT_R32_SFLOAT;
        attrs[2].offset = offsetof(MeshVert, skirt);

        vi.vertexBindingDescriptionCount = 1;
        vi.pVertexBindingDescriptions = &bind;
        vi.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
        vi.pVertexAttributeDescriptions = attrs.data();
    }

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo vp{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = 1;
    vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = polyMode;
    rs.lineWidth = 1.0f;
    rs.cullMode = cullMode;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = depthCompare;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAtt{};
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbAtt.blendEnable = enableBlend ? VK_TRUE : VK_FALSE;
    if (enableBlend)
    {
        // blend the color
        cbAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        cbAtt.colorBlendOp = VK_BLEND_OP_ADD;
        cbAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbAtt.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cbAtt;

    std::array<VkDynamicState, 2> dynStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = (uint32_t)dynStates.size();
    dyn.pDynamicStates = dynStates.data();

    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vp;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dyn;
    gp.layout = layout;
    gp.renderPass = renderPass;
    gp.subpass = 0;

    VkPipeline pipeline{};
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("vkCreateGraphicsPipelines failed");

    vkDestroyShaderModule(device, vs, nullptr);
    vkDestroyShaderModule(device, fs, nullptr);
    return pipeline;
}

static VkPipeline createGraphicsPipelineObjMesh(
    VkDevice device,
    VkRenderPass renderPass,
    VkPipelineLayout layout,
    VkExtent2D extent,
    const std::string &vsPath,
    const std::string &fsPath,
    bool depthWrite,
    VkCompareOp depthCompare,
    VkPolygonMode polyMode,
    VkCullModeFlags cullMode,
    bool depthTest = true,
    bool enableBlend = false)
{
    auto vsCode = readFileBinary(vsPath);
    auto fsCode = readFileBinary(fsPath);
    VkShaderModule vs = createShaderModule(device, vsCode);
    VkShaderModule fs = createShaderModule(device, fsCode);

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkVertexInputBindingDescription bind{};
    bind.binding = 0;
    bind.stride = sizeof(ObjVertex);
    bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attrs{};
    attrs[0].location = 0;
    attrs[0].binding = 0;
    attrs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[0].offset = 0;
    attrs[1].location = 1;
    attrs[1].binding = 0;
    attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attrs[1].offset = 12;
    attrs[2].location = 2;
    attrs[2].binding = 0;
    attrs[2].format = VK_FORMAT_R32G32_SFLOAT;
    attrs[2].offset = 24;

    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = (uint32_t)attrs.size();
    vi.pVertexAttributeDescriptions = attrs.data();

    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ia.primitiveRestartEnable = VK_FALSE;

    VkViewport vp{};
    vp.x = 0;
    vp.y = 0;
    vp.width = (float)extent.width;
    vp.height = (float)extent.height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    VkRect2D sc{};
    sc.offset = {0, 0};
    sc.extent = extent;

    VkPipelineViewportStateCreateInfo vpState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vpState.viewportCount = 1;
    vpState.pViewports = &vp;
    vpState.scissorCount = 1;
    vpState.pScissors = &sc;

    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.depthClampEnable = VK_FALSE;
    rs.rasterizerDiscardEnable = VK_FALSE;
    rs.polygonMode = polyMode;
    rs.cullMode = cullMode;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rs.depthBiasEnable = VK_FALSE;
    rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    ds.depthTestEnable = depthTest ? VK_TRUE : VK_FALSE;
    ds.depthWriteEnable = depthWrite ? VK_TRUE : VK_FALSE;
    ds.depthCompareOp = depthCompare;
    ds.depthBoundsTestEnable = VK_FALSE;
    ds.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState cbAtt{};
    cbAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    cbAtt.blendEnable = enableBlend ? VK_TRUE : VK_FALSE;
    if (enableBlend)
    {
        cbAtt.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        cbAtt.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbAtt.colorBlendOp = VK_BLEND_OP_ADD;
        cbAtt.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        cbAtt.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        cbAtt.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cbAtt;

    VkDynamicState dynStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dyn.dynamicStateCount = 2;
    dyn.pDynamicStates = dynStates;

    VkGraphicsPipelineCreateInfo ci{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.stageCount = 2;
    ci.pStages = stages;
    ci.pVertexInputState = &vi;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vpState;
    ci.pRasterizationState = &rs;
    ci.pMultisampleState = &ms;
    ci.pDepthStencilState = &ds;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &dyn;
    ci.layout = layout;
    ci.renderPass = renderPass;
    ci.subpass = 0;

    VkPipeline pipeline{};
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("vkCreateGraphicsPipelines(ducky obj file) failed");

    vkDestroyShaderModule(device, vs, nullptr);
    vkDestroyShaderModule(device, fs, nullptr);
    return pipeline;
}

static VkPipeline createComputePipeline(
    VkDevice device,
    VkPipelineLayout layout,
    const std::string &csPath)
{
    auto code = readFileBinary(csPath);
    VkShaderModule cs = createShaderModule(device, code);

    VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = cs;
    stage.pName = "main";

    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = stage;
    ci.layout = layout;

    VkPipeline p{};
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &ci, nullptr, &p) != VK_SUCCESS)
        throw std::runtime_error("vkCreateComputePipelines failed");

    vkDestroyShaderModule(device, cs, nullptr);
    return p;
}

static void imageBarrierGeneral(
    VkCommandBuffer cmd,
    VkImage image,
    VkImageAspectFlags aspect,
    VkAccessFlags srcAccess,
    VkAccessFlags dstAccess,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage,
    uint32_t mipLevels,
    uint32_t layers)
{
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange.aspectMask = aspect;
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = mipLevels;
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = layers;
    b.srcAccessMask = srcAccess;
    b.dstAccessMask = dstAccess;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
}

static void bufferBarrier(
    VkCommandBuffer cmd,
    VkBuffer buf,
    VkAccessFlags srcAccess,
    VkAccessFlags dstAccess,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage)
{
    VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    b.srcAccessMask = srcAccess;
    b.dstAccessMask = dstAccess;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.buffer = buf;
    b.offset = 0;
    b.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 1, &b, 0, nullptr);
}

int main(int argc, char **argv)
{
    fs::path exeDir = (argc > 0) ? fs::absolute(argv[0]).parent_path() : fs::current_path();
    fs::path spvDir = exeDir / "shaders_spv";
    fs::path assetsDir = exeDir / "assets";
    std::cout << "ExeDir: " << exeDir.string() << "\n";
    std::cout << "SPV:    " << spvDir.string() << "\n";
    std::cout << "Assets: " << assetsDir.string() << "\n";

    if (!glfwInit())
    {
        std::cerr << "GLFW init failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(1920, 1080, "Vulkan Ocean Render", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Window creation failed\n";
        glfwTerminate();
        return -1;
    }

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    VkContext ctx;
    bool enableValidation = true;
#ifndef NDEBUG
    enableValidation = true;
#else
    enableValidation = false;
#endif
    try
    {
        ctx.init(window, enableValidation);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Vulkan init error: " << e.what() << "\n";
        glfwTerminate();
        return -1;
    }

    VkDescriptorSetLayout uboSetLayout{};
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = 1;
        ci.pBindings = &b;
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &uboSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(ubo) failed");
    }

    VkDescriptorSetLayout texSetLayout{};
    {
        // 0 FFT
        // 1 HDR
        // 2 Foam
        // 3 SceneColor
        // 4 SceneDepth
        // 5 FFT detail
        // 6 Wake

        std::array<VkDescriptorSetLayoutBinding, 7> b{};
        for (uint32_t i = 0; i < (uint32_t)b.size(); i++)
        {
            b[i].binding = i;
            b[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b[i].descriptorCount = 1;
        }
        b[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        b[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        b[6].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = (uint32_t)b.size();
        ci.pBindings = b.data();
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &texSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout failed");
    }

    VkDescriptorSetLayout compSpectrumSetLayout{};
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = 1;
        ci.pBindings = &b;
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &compSpectrumSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(compSpectrum) failed");
    }

    VkDescriptorSetLayout comp2ImgSetLayout{};
    {
        std::array<VkDescriptorSetLayoutBinding, 2> b{};
        for (uint32_t i = 0; i < 2; i++)
        {
            b[i].binding = i;
            b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            b[i].descriptorCount = 1;
            b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = (uint32_t)b.size();
        ci.pBindings = b.data();
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &comp2ImgSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(comp2img) failed");
    }

    VkDescriptorSetLayout comp3ImgSetLayout{};
    {
        std::array<VkDescriptorSetLayoutBinding, 3> b{};
        for (uint32_t i = 0; i < 3; i++)
        {
            b[i].binding = i;
            b[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            b[i].descriptorCount = 1;
            b[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = (uint32_t)b.size();
        ci.pBindings = b.data();
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &comp3ImgSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(comp3img) failed");
    }

    // foam
    VkDescriptorSetLayout compFoamSetLayout{};
    {
        std::array<VkDescriptorSetLayoutBinding, 3> b{};
        b[0].binding = 0;
        b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b[0].descriptorCount = 1;
        b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        b[1].binding = 1;
        b[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b[1].descriptorCount = 1;
        b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        b[2].binding = 2;
        b[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        b[2].descriptorCount = 1;
        b[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = (uint32_t)b.size();
        ci.pBindings = b.data();
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &compFoamSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(compFoam) failed");
    }

    // spray
    VkDescriptorSetLayout compSpraySetLayout{};
    {
        std::array<VkDescriptorSetLayoutBinding, 3> b{};
        b[0].binding = 0;
        b[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b[0].descriptorCount = 1;
        b[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        b[1].binding = 1;
        b[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[1].descriptorCount = 1;
        b[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        b[2].binding = 2;
        b[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b[2].descriptorCount = 1;
        b[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = (uint32_t)b.size();
        ci.pBindings = b.data();
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &compSpraySetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(compSpray) failed");
    }

    VkDescriptorSetLayout spraySetLayout{};
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = 1;
        ci.pBindings = &b;
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &spraySetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(spray) failed");
    }

    VkDescriptorSetLayout taaSetLayout{};
    {
        std::array<VkDescriptorSetLayoutBinding, 4> b{};
        b[0].binding = 0;
        b[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        b[0].descriptorCount = 1;
        b[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        for (uint32_t i = 1; i < 4; i++)
        {
            b[i].binding = i;
            b[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b[i].descriptorCount = 1;
            b[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = (uint32_t)b.size();
        ci.pBindings = b.data();
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &taaSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(TAA) failed");
    }

    // hdr
    VkDescriptorSetLayout tonemapSetLayout{};
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding = 0;
        b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b.descriptorCount = 1;
        b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        ci.bindingCount = 1;
        ci.pBindings = &b;
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &tonemapSetLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorSetLayout(tonemap) failed");
    }

    VkPipelineLayout waterLayout{};
    {
        std::array<VkDescriptorSetLayout, 2> sets{uboSetLayout, texSetLayout};
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset = 0;
        pc.size = sizeof(WaterPush);

        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = (uint32_t)sets.size();
        ci.pSetLayouts = sets.data();
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &waterLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(water) failed");
    }

    VkPipelineLayout skyLayout{};
    {
        std::array<VkDescriptorSetLayout, 2> sets{uboSetLayout, texSetLayout};
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = (uint32_t)sets.size();
        ci.pSetLayouts = sets.data();
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &skyLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(sky) failed");
    }

    VkPipelineLayout boatLayout{};
    {
        std::array<VkDescriptorSetLayout, 2> sets{uboSetLayout, texSetLayout};
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset = 0;
        pc.size = sizeof(BoatPush);

        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = (uint32_t)sets.size();
        ci.pSetLayouts = sets.data();
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &boatLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(boat) failed");
    }

    VkPipelineLayout compSpectrumLayout{};
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc.offset = 0;
        pc.size = 32;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &compSpectrumSetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &compSpectrumLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(compSpectrum) failed");
    }

    VkPipelineLayout compBuildLayout{};
    {
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &comp2ImgSetLayout;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &compBuildLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(compBuild) failed");
    }

    VkPipelineLayout compIfftLayout{};
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc.offset = 0;
        pc.size = 16;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &comp2ImgSetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &compIfftLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(compIfft) failed");
    }

    VkPipelineLayout compCombineLayout{};
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc.offset = 0;
        pc.size = 16;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &comp3ImgSetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &compCombineLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(compCombine) failed");
    }

    VkPipelineLayout compFoamLayout{};
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc.offset = 0;
        pc.size = 48;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &compFoamSetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &compFoamLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(compFoam) failed");
    }

    VkPipelineLayout compSprayUpdateLayout{};
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc.offset = 0;
        pc.size = 16;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &compSpraySetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &compSprayUpdateLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(compSprayUpdate) failed");
    }

    VkPipelineLayout compSpraySpawnLayout{};
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pc.offset = 0;
        pc.size = 64;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &compSpraySetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &compSpraySpawnLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(compSpraySpawn) failed");
    }

    VkPipelineLayout sprayLayout{};
    {
        std::array<VkDescriptorSetLayout, 2> sets{uboSetLayout, spraySetLayout};
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = (uint32_t)sets.size();
        ci.pSetLayouts = sets.data();
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &sprayLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(spray) failed");
    }

    VkPipelineLayout taaLayout{};
    {
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &taaSetLayout;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &taaLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(taa) failed");
    }

    VkPipelineLayout tonemapLayout{};
    {
        VkPushConstantRange pc{};
        pc.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pc.offset = 0;
        pc.size = 4;
        VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        ci.setLayoutCount = 1;
        ci.pSetLayouts = &tonemapSetLayout;
        ci.pushConstantRangeCount = 1;
        ci.pPushConstantRanges = &pc;
        if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &tonemapLayout) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout(tonemap) failed");
    }

    // graphics pipelines
    VkPipeline waterFill{};
    VkPipeline waterLine{};
    VkPipeline skyMainPipe{};
    VkPipeline boatPipe{};
    VkPipeline sprayPipe{};
    VkPipeline taaPipe{};
    VkPipeline tonemapPipe{};

    VkPipeline csSpectrum{};
    VkPipeline csBuild{};
    VkPipeline csRows{};
    VkPipeline csCols{};
    VkPipeline csCombine{};
    VkPipeline csFoam{};
    VkPipeline csSprayUpdate{};
    VkPipeline csSpraySpawn{};

    const auto spv = [&](const char *name)
    { return (spvDir / name).string(); };

    try
    {
        csSpectrum = createComputePipeline(ctx.device, compSpectrumLayout, spv("spectrum.comp.spv"));
        csBuild = createComputePipeline(ctx.device, compBuildLayout, spv("build_tiles.comp.spv"));
        csRows = createComputePipeline(ctx.device, compIfftLayout, spv("ifft_rows.comp.spv"));
        csCols = createComputePipeline(ctx.device, compIfftLayout, spv("ifft_cols.comp.spv"));
        csCombine = createComputePipeline(ctx.device, compCombineLayout, spv("fft_combine.comp.spv"));
        csFoam = createComputePipeline(ctx.device, compFoamLayout, spv("foam.comp.spv"));
        csSprayUpdate = createComputePipeline(ctx.device, compSprayUpdateLayout, spv("spray_update.comp.spv"));
        csSpraySpawn = createComputePipeline(ctx.device, compSpraySpawnLayout, spv("spray_spawn.comp.spv"));
    }
    catch (const std::exception &e)
    {
        std::cerr << "Pipeline error: " << e.what() << "\n";
        ctx.cleanup();
        glfwTerminate();
        return -1;
    }

    VkDescriptorPool gfxPool{};
    {
        // UBOs: GlobalUBO per frame + TAA UBO per frame
        // combined samplers: water/sky + scene refs + TAA + tonemap
        // storage buffers: spray particles
        std::array<VkDescriptorPoolSize, 3> sizes{};
        sizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VkContext::kMaxFrames * 2 + 8};
        sizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128};
        sizes[2] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8};

        VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        ci.maxSets = 64;
        ci.poolSizeCount = (uint32_t)sizes.size();
        ci.pPoolSizes = sizes.data();
        if (vkCreateDescriptorPool(ctx.device, &ci, nullptr, &gfxPool) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorPool(gfx) failed");
    }

    VkDescriptorPool compPool{};
    {
        // storage images: FFT chain + foam output
        // combined samplers: foam reads FFT + foamPrev, spray spawn reads FFT
        // storage buffers: spray particles + counter
        std::array<VkDescriptorPoolSize, 3> sizes{};
        sizes[0] = {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32};
        sizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
        sizes[2] = {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8};

        VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        ci.maxSets = 16;
        ci.poolSizeCount = (uint32_t)sizes.size();
        ci.pPoolSizes = sizes.data();
        if (vkCreateDescriptorPool(ctx.device, &ci, nullptr, &compPool) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDescriptorPool(comp) failed");
    }

    // 0 swell
    AllocatedImage texH0 = createImage2D(
        ctx.phys, ctx.device,
        FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    AllocatedImage texB0_0 = createImage2D(
        ctx.phys, ctx.device,
        3 * FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    AllocatedImage texB1_0 = createImage2D(
        ctx.phys, ctx.device,
        3 * FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // 1 wind
    AllocatedImage texH1 = createImage2D(
        ctx.phys, ctx.device,
        FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    AllocatedImage texB0_1 = createImage2D(
        ctx.phys, ctx.device,
        3 * FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    AllocatedImage texB1_1 = createImage2D(
        ctx.phys, ctx.device,
        3 * FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // displacement field
    AllocatedImage texBCombined = createImage2D(
        ctx.phys, ctx.device,
        3 * FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R32G32_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    {
        VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        transitionImageLayout(cmd, texH0.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        transitionImageLayout(cmd, texB0_0.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        transitionImageLayout(cmd, texB1_0.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        transitionImageLayout(cmd, texH1.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        transitionImageLayout(cmd, texB0_1.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        transitionImageLayout(cmd, texB1_1.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        transitionImageLayout(cmd, texBCombined.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);
    }

    VkSampler fftSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, false, 1.0f);

    // foam ping pong
    AllocatedImage foamImg[2]{};
    foamImg[0] = createImage2D(
        ctx.phys, ctx.device,
        FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    foamImg[1] = createImage2D(
        ctx.phys, ctx.device,
        FREQ_SIZE, FREQ_SIZE,
        1,
        VK_FORMAT_R16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    // transition to GENERAL and clear to 0
    {
        VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        transitionImageLayout(cmd, foamImg[0].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        transitionImageLayout(cmd, foamImg[1].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        VkClearColorValue zero{{0.0f, 0.0f, 0.0f, 0.0f}};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        vkCmdClearColorImage(cmd, foamImg[0].image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
        vkCmdClearColorImage(cmd, foamImg[1].image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);
    }

    VkSampler foamSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, false, 1.0f);

    struct ParticleCPU
    {
        glm::vec4 posLife;
        glm::vec4 velSeed;
    };
    AllocatedBuffer sprayBuf = createBuffer(ctx.phys, ctx.device,
                                            VkDeviceSize(MAX_PARTICLES) * sizeof(ParticleCPU),
                                            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    AllocatedBuffer sprayCounter = createBuffer(ctx.phys, ctx.device,
                                                sizeof(uint32_t),
                                                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    {
        std::vector<ParticleCPU> zeros;
        zeros.resize(MAX_PARTICLES);
        AllocatedBuffer st = createBuffer(ctx.phys, ctx.device,
                                          VkDeviceSize(MAX_PARTICLES) * sizeof(ParticleCPU),
                                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        void *map = nullptr;
        vkMapMemory(ctx.device, st.memory, 0, st.size, 0, &map);
        std::memcpy(map, zeros.data(), (size_t)st.size);
        vkUnmapMemory(ctx.device, st.memory);

        VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        VkBufferCopy cpy{0, 0, st.size};
        vkCmdCopyBuffer(cmd, st.buffer, sprayBuf.buffer, 1, &cpy);
        vkCmdFillBuffer(cmd, sprayCounter.buffer, 0, sizeof(uint32_t), 0);
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);

        destroyBuffer(ctx.device, st);
    }

    // load hdr
    AllocatedImage hdrImg{};
    VkSampler hdrSampler{};
    float envMaxMip = 0.0f;

    try
    {
        const fs::path hdrPath = assetsDir / "sky.hdr";
        int hdrW = 0, hdrH = 0;
        std::vector<float> hdrRGBA = loadRadianceHDR_RGBA32F(hdrPath.string(), hdrW, hdrH);
        const uint32_t mipLevels = mipCount2D((uint32_t)hdrW, (uint32_t)hdrH);
        envMaxMip = float(mipLevels - 1);

        std::vector<uint16_t> halfPixels;
        halfPixels.resize(size_t(hdrW) * size_t(hdrH) * 4u);
        for (size_t i = 0; i < halfPixels.size(); ++i)
            halfPixels[i] = floatToHalf(hdrRGBA[i]);

        VkDeviceSize uploadSize = VkDeviceSize(halfPixels.size() * sizeof(uint16_t));
        AllocatedBuffer staging = createBuffer(ctx.phys, ctx.device, uploadSize,
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        void *map = nullptr;
        vkMapMemory(ctx.device, staging.memory, 0, uploadSize, 0, &map);
        std::memcpy(map, halfPixels.data(), (size_t)uploadSize);
        vkUnmapMemory(ctx.device, staging.memory);

        hdrImg = createImage2D(ctx.phys, ctx.device,
                               (uint32_t)hdrW, (uint32_t)hdrH,
                               mipLevels,
                               VK_FORMAT_R16G16B16A16_SFLOAT,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT);

        VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        transitionImageLayout(cmd, hdrImg.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
        copyBufferToImage(cmd, staging.buffer, hdrImg.image, (uint32_t)hdrW, (uint32_t)hdrH);
        generateMipmaps(ctx.phys, cmd, hdrImg.image, hdrImg.format, hdrW, hdrH, mipLevels);
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);

        destroyBuffer(ctx.device, staging);

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(ctx.phys, &props);
        hdrSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, (float)mipLevels, true, props.limits.maxSamplerAnisotropy);

        std::cout << "Loaded HDR: " << hdrPath.string() << " (" << hdrW << "x" << hdrH << ") mips=" << mipLevels << "\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << "HDR load failed: " << e.what() << "\n";
        std::cerr << "Put your sky.hdr at: " << (assetsDir / "sky.hdr").string() << "\n";
    }

    // evil box in case hdr missing
    if (!hdrImg.image)
    {
        envMaxMip = 0.0f;
        const uint32_t w = 1, h = 1, mips = 1;
        uint16_t px[4] = {floatToHalf(0.0f), floatToHalf(0.0f), floatToHalf(0.0f), floatToHalf(1.0f)};
        AllocatedBuffer staging = createBuffer(ctx.phys, ctx.device, sizeof(px),
                                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        void *map = nullptr;
        vkMapMemory(ctx.device, staging.memory, 0, sizeof(px), 0, &map);
        std::memcpy(map, px, sizeof(px));
        vkUnmapMemory(ctx.device, staging.memory);

        hdrImg = createImage2D(ctx.phys, ctx.device,
                               w, h,
                               mips,
                               VK_FORMAT_R16G16B16A16_SFLOAT,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               VK_IMAGE_ASPECT_COLOR_BIT);

        VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        transitionImageLayout(cmd, hdrImg.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        copyBufferToImage(cmd, staging.buffer, hdrImg.image, w, h);
        transitionImageLayout(cmd, hdrImg.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);

        destroyBuffer(ctx.device, staging);
        hdrSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, false, 1.0f);
    }

    // cubemap
    AllocatedImage envCube{};
    VkSampler envCubeSampler{};
    if (hdrImg.image)
    {
        const uint32_t cubeSize = 512;
        envCube = createImage2D(ctx.phys, ctx.device,
                                cubeSize, cubeSize,
                                1,
                                VK_FORMAT_R16G16B16A16_SFLOAT,
                                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_SAMPLE_COUNT_1_BIT,
                                VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
                                6);

        envCubeSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, false, 1.0f);

        // view per face
        VkImageView faceViews[6]{};
        for (uint32_t i = 0; i < 6; i++)
        {
            faceViews[i] = createImageView(ctx.device, envCube.image, envCube.format, VK_IMAGE_ASPECT_COLOR_BIT, 1,
                                           VK_IMAGE_VIEW_TYPE_2D, 0, i, 1);
        }

        // color pass
        VkAttachmentDescription color{};
        color.format = envCube.format;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference cref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &cref;

        VkRenderPass rp{};
        VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpci.attachmentCount = 1;
        rpci.pAttachments = &color;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &sub;
        if (vkCreateRenderPass(ctx.device, &rpci, nullptr, &rp) != VK_SUCCESS)
            throw std::runtime_error("vkCreateRenderPass(cubemap) failed");

        VkFramebuffer fbs[6]{};
        for (uint32_t i = 0; i < 6; i++)
        {
            VkImageView att = faceViews[i];
            VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbi.renderPass = rp;
            fbi.attachmentCount = 1;
            fbi.pAttachments = &att;
            fbi.width = cubeSize;
            fbi.height = cubeSize;
            fbi.layers = 1;
            if (vkCreateFramebuffer(ctx.device, &fbi, nullptr, &fbs[i]) != VK_SUCCESS)
                throw std::runtime_error("vkCreateFramebuffer(cubemap) failed");
        }

        VkDescriptorSetLayout capSetLayout{};
        {
            VkDescriptorSetLayoutBinding b{};
            b.binding = 0;
            b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            b.descriptorCount = 1;
            b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
            VkDescriptorSetLayoutCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            ci.bindingCount = 1;
            ci.pBindings = &b;
            vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &capSetLayout);
        }

        VkPipelineLayout capLayout{};
        {
            VkPushConstantRange pc{};
            pc.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            pc.offset = 0;
            pc.size = sizeof(glm::mat4) * 2;
            VkPipelineLayoutCreateInfo ci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            ci.setLayoutCount = 1;
            ci.pSetLayouts = &capSetLayout;
            ci.pushConstantRangeCount = 1;
            ci.pPushConstantRanges = &pc;
            vkCreatePipelineLayout(ctx.device, &ci, nullptr, &capLayout);
        }

        VkPipeline capPipe = createGraphicsPipeline(ctx.device, rp, capLayout, {cubeSize, cubeSize},
                                                    spv("cube_capture.vert.spv"), spv("equirect_to_cubemap.frag.spv"),
                                                    false,
                                                    false, VK_COMPARE_OP_ALWAYS,
                                                    VK_POLYGON_MODE_FILL,
                                                    VK_CULL_MODE_NONE,
                                                    false);

        VkDescriptorPool capPool{};
        {
            VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
            VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            ci.maxSets = 1;
            ci.poolSizeCount = 1;
            ci.pPoolSizes = &ps;
            vkCreateDescriptorPool(ctx.device, &ci, nullptr, &capPool);
        }

        VkDescriptorSet capSet{};
        {
            VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            ai.descriptorPool = capPool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts = &capSetLayout;
            vkAllocateDescriptorSets(ctx.device, &ai, &capSet);

            VkDescriptorImageInfo ii{};
            ii.sampler = hdrSampler;
            ii.imageView = hdrImg.view;
            ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            w.dstSet = capSet;
            w.dstBinding = 0;
            w.descriptorCount = 1;
            w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w.pImageInfo = &ii;
            vkUpdateDescriptorSets(ctx.device, 1, &w, 0, nullptr);
        }

        // Capture matrices
        glm::mat4 captureProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
        captureProj[1][1] *= -1.0f;

        glm::mat4 captureViews[6] = {
            glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
            glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
            glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
            glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
            glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
            glm::lookAt(glm::vec3(0, 0, 0), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)),
        };

        // render faces
        VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);

        VkClearValue clear{};
        clear.color = {{0.f, 0.f, 0.f, 1.f}};

        VkViewport vp{};
        vp.x = 0;
        vp.y = 0;
        vp.width = (float)cubeSize;
        vp.height = (float)cubeSize;
        vp.minDepth = 0.f;
        vp.maxDepth = 1.f;
        VkRect2D sc{};
        sc.extent = {cubeSize, cubeSize};

        for (uint32_t face = 0; face < 6; ++face)
        {
            VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            rbi.renderPass = rp;
            rbi.framebuffer = fbs[face];
            rbi.renderArea.extent = {cubeSize, cubeSize};
            rbi.clearValueCount = 1;
            rbi.pClearValues = &clear;

            vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, capPipe);
            vkCmdSetViewport(cmd, 0, 1, &vp);
            vkCmdSetScissor(cmd, 0, 1, &sc);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, capLayout, 0, 1, &capSet, 0, nullptr);

            struct
            {
                glm::mat4 view;
                glm::mat4 proj;
            } pc{captureViews[face], captureProj};
            vkCmdPushConstants(cmd, capLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pc), &pc);

            vkCmdDraw(cmd, 36, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }

        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);

        // clean mess
        vkDestroyPipeline(ctx.device, capPipe, nullptr);
        vkDestroyPipelineLayout(ctx.device, capLayout, nullptr);
        vkDestroyDescriptorPool(ctx.device, capPool, nullptr);
        vkDestroyDescriptorSetLayout(ctx.device, capSetLayout, nullptr);
        for (uint32_t i = 0; i < 6; i++)
        {
            vkDestroyFramebuffer(ctx.device, fbs[i], nullptr);
            vkDestroyImageView(ctx.device, faceViews[i], nullptr);
        }
        vkDestroyRenderPass(ctx.device, rp, nullptr);

        std::cout << "Built env cubemap (" << cubeSize << "^2)\n";
    }

    // render in the skybox and use the depth for thickness and apply the beer-lambert
    AllocatedImage sceneColor{};
    AllocatedImage sceneDepth{};
    VkSampler sceneColorSampler{};
    VkSampler sceneDepthSampler{};
    VkRenderPass sceneRenderPass{};
    VkFramebuffer sceneFramebuffer{};
    VkPipeline sceneSkyPipe{};

    auto destroySceneTargets = [&]
    {
        if (sceneFramebuffer)
            vkDestroyFramebuffer(ctx.device, sceneFramebuffer, nullptr);
        sceneFramebuffer = VK_NULL_HANDLE;
        if (sceneColor.image)
            destroyImage(ctx.device, sceneColor);
        sceneColor = {};
        if (sceneDepth.image)
            destroyImage(ctx.device, sceneDepth);
        sceneDepth = {};
    };

    // render pass
    {
        VkAttachmentDescription color{};
        color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription depth{};
        depth.format = ctx.depthFormat;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &colorRef;
        sub.pDepthStencilAttachment = &depthRef;

        // make the writes visible to later fragment sampling.
        std::array<VkSubpassDependency, 2> deps{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        std::array<VkAttachmentDescription, 2> atts{color, depth};
        VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpci.attachmentCount = (uint32_t)atts.size();
        rpci.pAttachments = atts.data();
        rpci.subpassCount = 1;
        rpci.pSubpasses = &sub;
        rpci.dependencyCount = (uint32_t)deps.size();
        rpci.pDependencies = deps.data();
        if (vkCreateRenderPass(ctx.device, &rpci, nullptr, &sceneRenderPass) != VK_SUCCESS)
            throw std::runtime_error("vkCreateRenderPass(scene) failed");
    }

    sceneColorSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, false, 1.0f);
    sceneDepthSampler = createSampler(ctx.device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, false, 1.0f);

    auto rebuildSceneTargets = [&](VkExtent2D extent)
    {
        destroySceneTargets();

        sceneColor = createImage2D(
            ctx.phys, ctx.device,
            extent.width, extent.height,
            1,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);

        sceneDepth = createImage2D(
            ctx.phys, ctx.device,
            extent.width, extent.height,
            1,
            ctx.depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        transitionImageLayout(cmd, sceneColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        transitionImageLayout(cmd, sceneDepth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);

        VkImageView atts[] = {sceneColor.view, sceneDepth.view};
        VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbi.renderPass = sceneRenderPass;
        fbi.attachmentCount = 2;
        fbi.pAttachments = atts;
        fbi.width = extent.width;
        fbi.height = extent.height;
        fbi.layers = 1;
        if (vkCreateFramebuffer(ctx.device, &fbi, nullptr, &sceneFramebuffer) != VK_SUCCESS)
            throw std::runtime_error("vkCreateFramebuffer(scene) failed");
    };

    rebuildSceneTargets(ctx.swapExtent);

    sceneSkyPipe = createGraphicsPipeline(ctx.device, sceneRenderPass, skyLayout, ctx.swapExtent,
                                          spv("skybox.vert.spv"), spv("skybox_scene.frag.spv"),
                                          false,
                                          false, VK_COMPARE_OP_LESS_OR_EQUAL,
                                          VK_POLYGON_MODE_FILL,
                                          VK_CULL_MODE_NONE,
                                          true);

    // render maincoin and apply TAA
    AllocatedImage mainColor{};
    AllocatedImage mainDepth{};
    VkSampler mainColorSampler{};
    VkSampler mainDepthSampler{};
    VkRenderPass mainRenderPass{};
    VkFramebuffer mainFramebuffer{};

    auto destroyMainTargets = [&]
    {
        if (mainFramebuffer)
            vkDestroyFramebuffer(ctx.device, mainFramebuffer, nullptr);
        mainFramebuffer = VK_NULL_HANDLE;
        if (mainColor.image)
            destroyImage(ctx.device, mainColor);
        mainColor = {};
        if (mainDepth.image)
            destroyImage(ctx.device, mainDepth);
        mainDepth = {};
    };

    // MAIN PASS
    {
        VkAttachmentDescription color{};
        color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription depth{};
        depth.format = ctx.depthFormat;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &colorRef;
        sub.pDepthStencilAttachment = &depthRef;

        std::array<VkSubpassDependency, 2> deps{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        std::array<VkAttachmentDescription, 2> atts{color, depth};
        VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpci.attachmentCount = (uint32_t)atts.size();
        rpci.pAttachments = atts.data();
        rpci.subpassCount = 1;
        rpci.pSubpasses = &sub;
        rpci.dependencyCount = (uint32_t)deps.size();
        rpci.pDependencies = deps.data();
        if (vkCreateRenderPass(ctx.device, &rpci, nullptr, &mainRenderPass) != VK_SUCCESS)
            throw std::runtime_error("vkCreateRenderPass(main) failed");
    }

    mainColorSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, false, 1.0f);
    mainDepthSampler = createSampler(ctx.device, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, false, 1.0f);

    auto rebuildMainTargets = [&](VkExtent2D extent)
    {
        destroyMainTargets();
        mainColor = createImage2D(
            ctx.phys, ctx.device,
            extent.width, extent.height,
            1,
            VK_FORMAT_R16G16B16A16_SFLOAT,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT);
        mainDepth = createImage2D(
            ctx.phys, ctx.device,
            extent.width, extent.height,
            1,
            ctx.depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT);

        VkCommandBuffer cmd2 = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        transitionImageLayout(cmd2, mainColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        transitionImageLayout(cmd2, mainDepth.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd2);

        VkImageView atts[] = {mainColor.view, mainDepth.view};
        VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        fbi.renderPass = mainRenderPass;
        fbi.attachmentCount = 2;
        fbi.pAttachments = atts;
        fbi.width = extent.width;
        fbi.height = extent.height;
        fbi.layers = 1;
        if (vkCreateFramebuffer(ctx.device, &fbi, nullptr, &mainFramebuffer) != VK_SUCCESS)
            throw std::runtime_error("vkCreateFramebuffer(main) failed");
    };

    rebuildMainTargets(ctx.swapExtent);

    // TAA
    AllocatedImage taaHist[2]{};
    VkFramebuffer taaFB[2]{};
    VkRenderPass taaRenderPass{};
    VkSampler taaSampler{};

    auto destroyTaaTargets = [&]
    {
        for (int i = 0; i < 2; i++)
            if (taaFB[i])
                vkDestroyFramebuffer(ctx.device, taaFB[i], nullptr);
        taaFB[0] = taaFB[1] = VK_NULL_HANDLE;
        for (int i = 0; i < 2; i++)
            if (taaHist[i].image)
                destroyImage(ctx.device, taaHist[i]);
        taaHist[0] = taaHist[1] = {};
    };

    {
        VkAttachmentDescription color{};
        color.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        color.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference cref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        VkSubpassDescription sub{};
        sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        sub.colorAttachmentCount = 1;
        sub.pColorAttachments = &cref;

        std::array<VkSubpassDependency, 2> deps{};
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rpci{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpci.attachmentCount = 1;
        rpci.pAttachments = &color;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &sub;
        rpci.dependencyCount = (uint32_t)deps.size();
        rpci.pDependencies = deps.data();
        if (vkCreateRenderPass(ctx.device, &rpci, nullptr, &taaRenderPass) != VK_SUCCESS)
            throw std::runtime_error("vkCreateRenderPass(TAA) failed");
    }

    taaSampler = createSampler(ctx.device, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, false, 1.0f);

    auto rebuildTaaTargets = [&](VkExtent2D extent)
    {
        destroyTaaTargets();
        for (int i = 0; i < 2; i++)
        {
            taaHist[i] = createImage2D(ctx.phys, ctx.device,
                                       extent.width, extent.height,
                                       1,
                                       VK_FORMAT_R16G16B16A16_SFLOAT,
                                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_IMAGE_ASPECT_COLOR_BIT);
        }

        VkCommandBuffer cmd2 = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
        VkClearColorValue zero{{0.0f, 0.0f, 0.0f, 0.0f}};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        for (int i = 0; i < 2; i++)
        {
            transitionImageLayout(cmd2, taaHist[i].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
            vkCmdClearColorImage(cmd2, taaHist[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &zero, 1, &range);
            transitionImageLayout(cmd2, taaHist[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }
        endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd2);

        for (int i = 0; i < 2; i++)
        {
            VkImageView att = taaHist[i].view;
            VkFramebufferCreateInfo fbi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fbi.renderPass = taaRenderPass;
            fbi.attachmentCount = 1;
            fbi.pAttachments = &att;
            fbi.width = extent.width;
            fbi.height = extent.height;
            fbi.layers = 1;
            if (vkCreateFramebuffer(ctx.device, &fbi, nullptr, &taaFB[i]) != VK_SUCCESS)
                throw std::runtime_error("vkCreateFramebuffer(TAA) failed");
        }
    };

    rebuildTaaTargets(ctx.swapExtent);

    uint32_t taaParity = 0;

    try
    {
        // sky + water
        skyMainPipe = createGraphicsPipeline(ctx.device, mainRenderPass, skyLayout, ctx.swapExtent,
                                             spv("skybox.vert.spv"), spv("skybox_scene.frag.spv"),
                                             false,
                                             false, VK_COMPARE_OP_LESS_OR_EQUAL,
                                             VK_POLYGON_MODE_FILL,
                                             VK_CULL_MODE_NONE,
                                             true);

        // boat
        boatPipe = createGraphicsPipelineObjMesh(ctx.device, mainRenderPass, boatLayout, ctx.swapExtent,
                                                 spv("boat.vert.spv"), spv("boat.frag.spv"),
                                                 true, VK_COMPARE_OP_LESS_OR_EQUAL,
                                                 VK_POLYGON_MODE_FILL,
                                                 VK_CULL_MODE_NONE,
                                                 true);

        waterFill = createGraphicsPipeline(ctx.device, mainRenderPass, waterLayout, ctx.swapExtent,
                                           spv("water.vert.spv"), spv("water.frag.spv"),
                                           true,
                                           true, VK_COMPARE_OP_LESS,
                                           VK_POLYGON_MODE_FILL,
                                           VK_CULL_MODE_NONE,
                                           true);

        try
        {
            waterLine = createGraphicsPipeline(ctx.device, mainRenderPass, waterLayout, ctx.swapExtent,
                                               spv("water.vert.spv"), spv("water.frag.spv"),
                                               true,
                                               true, VK_COMPARE_OP_LESS,
                                               VK_POLYGON_MODE_LINE,
                                               VK_CULL_MODE_NONE,
                                               true);
        }
        catch (...)
        {
            waterLine = VK_NULL_HANDLE;
        }

        // sporay
        sprayPipe = createGraphicsPipeline(ctx.device, mainRenderPass, sprayLayout, ctx.swapExtent,
                                           spv("spray.vert.spv"), spv("spray.frag.spv"),
                                           false,
                                           false, VK_COMPARE_OP_LESS_OR_EQUAL,
                                           VK_POLYGON_MODE_FILL,
                                           VK_CULL_MODE_NONE,
                                           true,
                                           true);

        // TAA
        taaPipe = createGraphicsPipeline(ctx.device, taaRenderPass, taaLayout, ctx.swapExtent,
                                         spv("fullscreen.vert.spv"), spv("taa.frag.spv"),
                                         false,
                                         false, VK_COMPARE_OP_ALWAYS,
                                         VK_POLYGON_MODE_FILL,
                                         VK_CULL_MODE_NONE,
                                         false);

        // tonemap to swapchain
        tonemapPipe = createGraphicsPipeline(ctx.device, ctx.renderPass, tonemapLayout, ctx.swapExtent,
                                             spv("fullscreen.vert.spv"), spv("tonemap.frag.spv"),
                                             false,
                                             false, VK_COMPARE_OP_ALWAYS,
                                             VK_POLYGON_MODE_FILL,
                                             VK_CULL_MODE_NONE,
                                             false);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Graphics pipeline error: " << e.what() << "\n";
        ctx.cleanup();
        glfwTerminate();
        return -1;
    }

    struct WaterMesh
    {
        AllocatedBuffer vbo{};
        AllocatedBuffer ibo{};
        uint32_t indexCount = 0;
    };

    auto buildWaterMesh = [&](int GRID_N) -> WaterMesh
    {
        std::vector<MeshVert> verts;
        verts.reserve((GRID_N + 1) * (GRID_N + 1) + (GRID_N + 1) * 4);

        auto idx = [GRID_N](int x, int z) -> uint32_t
        { return (uint32_t)(z * (GRID_N + 1) + x); };

        for (int z = 0; z <= GRID_N; z++)
        {
            for (int x = 0; x <= GRID_N; x++)
            {
                float u = (float)x / (float)GRID_N;
                float v = (float)z / (float)GRID_N;
                float px = (u - 0.5f) * PATCH_SIZE;
                float pz = (v - 0.5f) * PATCH_SIZE;
                MeshVert mv{};
                mv.xz[0] = px;
                mv.xz[1] = pz;
                mv.uv[0] = u;
                mv.uv[1] = v;
                mv.skirt = 0.0f;
                verts.push_back(mv);
            }
        }

        // hide cracks by adding skirt at the edges
        const uint32_t baseCount = (uint32_t)verts.size();
        std::vector<int32_t> skirtMap(baseCount, -1);

        auto addSkirt = [&](int x, int z)
        {
            uint32_t vi = idx(x, z);
            if (skirtMap[vi] >= 0)
                return;
            MeshVert mv = verts[vi];
            mv.skirt = 1.0f;
            skirtMap[vi] = (int32_t)verts.size();
            verts.push_back(mv);
        };

        for (int x = 0; x <= GRID_N; x++)
        {
            addSkirt(x, 0);
            addSkirt(x, GRID_N);
        }
        for (int z = 0; z <= GRID_N; z++)
        {
            addSkirt(0, z);
            addSkirt(GRID_N, z);
        }

        std::vector<uint32_t> indices;
        indices.reserve(GRID_N * GRID_N * 6 + GRID_N * 4 * 6);

        for (int z = 0; z < GRID_N; z++)
        {
            for (int x = 0; x < GRID_N; x++)
            {
                uint32_t i0 = idx(x, z);
                uint32_t i1 = idx(x + 1, z);
                uint32_t i2 = idx(x, z + 1);
                uint32_t i3 = idx(x + 1, z + 1);
                indices.push_back(i0);
                indices.push_back(i2);
                indices.push_back(i1);
                indices.push_back(i1);
                indices.push_back(i2);
                indices.push_back(i3);
            }
        }

        auto skirtOf = [&](int x, int z) -> uint32_t
        {
            uint32_t vi = idx(x, z);
            return (uint32_t)skirtMap[vi];
        };

        // bot edge
        for (int x = 0; x < GRID_N; x++)
        {
            uint32_t t0 = idx(x, 0), t1 = idx(x + 1, 0);
            uint32_t b0 = skirtOf(x, 0), b1 = skirtOf(x + 1, 0);
            indices.push_back(t0);
            indices.push_back(b0);
            indices.push_back(t1);
            indices.push_back(t1);
            indices.push_back(b0);
            indices.push_back(b1);
        }
        // top edge
        for (int x = 0; x < GRID_N; x++)
        {
            uint32_t t0 = idx(x, GRID_N), t1 = idx(x + 1, GRID_N);
            uint32_t b0 = skirtOf(x, GRID_N), b1 = skirtOf(x + 1, GRID_N);
            indices.push_back(t1);
            indices.push_back(b0);
            indices.push_back(t0);
            indices.push_back(b1);
            indices.push_back(b0);
            indices.push_back(t1);
        }
        // left
        for (int z = 0; z < GRID_N; z++)
        {
            uint32_t t0 = idx(0, z), t1 = idx(0, z + 1);
            uint32_t b0 = skirtOf(0, z), b1 = skirtOf(0, z + 1);
            indices.push_back(t1);
            indices.push_back(b0);
            indices.push_back(t0);
            indices.push_back(b1);
            indices.push_back(b0);
            indices.push_back(t1);
        }
        // right
        for (int z = 0; z < GRID_N; z++)
        {
            uint32_t t0 = idx(GRID_N, z), t1 = idx(GRID_N, z + 1);
            uint32_t b0 = skirtOf(GRID_N, z), b1 = skirtOf(GRID_N, z + 1);
            indices.push_back(t0);
            indices.push_back(b0);
            indices.push_back(t1);
            indices.push_back(t1);
            indices.push_back(b0);
            indices.push_back(b1);
        }

        WaterMesh out{};
        out.indexCount = (uint32_t)indices.size();

        out.vbo = createBuffer(ctx.phys, ctx.device,
                               VkDeviceSize(verts.size() * sizeof(MeshVert)),
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        out.ibo = createBuffer(ctx.phys, ctx.device,
                               VkDeviceSize(indices.size() * sizeof(uint32_t)),
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        {
            VkDeviceSize vSize = out.vbo.size;
            VkDeviceSize iSize = out.ibo.size;
            AllocatedBuffer stV = createBuffer(ctx.phys, ctx.device, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            AllocatedBuffer stI = createBuffer(ctx.phys, ctx.device, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void *map = nullptr;
            vkMapMemory(ctx.device, stV.memory, 0, vSize, 0, &map);
            std::memcpy(map, verts.data(), (size_t)vSize);
            vkUnmapMemory(ctx.device, stV.memory);

            vkMapMemory(ctx.device, stI.memory, 0, iSize, 0, &map);
            std::memcpy(map, indices.data(), (size_t)iSize);
            vkUnmapMemory(ctx.device, stI.memory);

            VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
            VkBufferCopy vc{0, 0, vSize};
            vkCmdCopyBuffer(cmd, stV.buffer, out.vbo.buffer, 1, &vc);
            VkBufferCopy ic{0, 0, iSize};
            vkCmdCopyBuffer(cmd, stI.buffer, out.ibo.buffer, 1, &ic);
            endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);

            destroyBuffer(ctx.device, stV);
            destroyBuffer(ctx.device, stI);
        }

        return out;
    };

    // water mesh
    WaterMesh meshHi = buildWaterMesh(256);
    WaterMesh meshMid = buildWaterMesh(128);
    WaterMesh meshLo = buildWaterMesh(64);

    // duck obj mesh
    struct ObjMesh
    {
        AllocatedBuffer vbo{};
        AllocatedBuffer ibo{};
        uint32_t indexCount = 0;
    } duckMesh{};

    {
        fs::path duckPath = assetsDir / "duck.obj";
        std::vector<ObjVertex> verts;
        std::vector<uint32_t> inds;
        ObjBounds bounds{};
        if (!loadObjTriangulated(duckPath.string(), verts, inds, &bounds, true))
        {
            std::cerr << "FATAL: could not load duck.obj at " << duckPath.string() << "\n";
            ctx.cleanup();
            glfwTerminate();
            return -1;
        }

        // normalize da duck
        float cx = 0.5f * (bounds.minx + bounds.maxx);
        float cz = 0.5f * (bounds.minz + bounds.maxz);
        float minY = bounds.miny;
        float sx = (bounds.maxx - bounds.minx);
        float sy = (bounds.maxy - bounds.miny);
        float sz = (bounds.maxz - bounds.minz);
        float maxDim = std::max(sx, std::max(sy, sz));
        float inv = (maxDim > 1e-6f) ? (1.0f / maxDim) : 1.0f;

        for (auto &v : verts)
        {
            v.px = (v.px - cx) * inv;
            v.py = (v.py - minY) * inv;
            v.pz = (v.pz - cz) * inv;

            float l = std::sqrt(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz);
            if (l > 1e-6f)
            {
                v.nx /= l;
                v.ny /= l;
                v.nz /= l;
            }
        }

        duckMesh.indexCount = (uint32_t)inds.size();

        duckMesh.vbo = createBuffer(ctx.phys, ctx.device,
                                    VkDeviceSize(verts.size() * sizeof(ObjVertex)),
                                    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        duckMesh.ibo = createBuffer(ctx.phys, ctx.device,
                                    VkDeviceSize(inds.size() * sizeof(uint32_t)),
                                    VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // upload
        {
            VkDeviceSize vSize = duckMesh.vbo.size;
            VkDeviceSize iSize = duckMesh.ibo.size;

            AllocatedBuffer stV = createBuffer(ctx.phys, ctx.device, vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            AllocatedBuffer stI = createBuffer(ctx.phys, ctx.device, iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            void *map = nullptr;
            vkMapMemory(ctx.device, stV.memory, 0, vSize, 0, &map);
            std::memcpy(map, verts.data(), (size_t)vSize);
            vkUnmapMemory(ctx.device, stV.memory);

            vkMapMemory(ctx.device, stI.memory, 0, iSize, 0, &map);
            std::memcpy(map, inds.data(), (size_t)iSize);
            vkUnmapMemory(ctx.device, stI.memory);

            VkCommandBuffer cmd = beginSingleTimeCommands(ctx.device, ctx.cmdPool);
            VkBufferCopy vc{0, 0, vSize};
            vkCmdCopyBuffer(cmd, stV.buffer, duckMesh.vbo.buffer, 1, &vc);
            VkBufferCopy ic{0, 0, iSize};
            vkCmdCopyBuffer(cmd, stI.buffer, duckMesh.ibo.buffer, 1, &ic);
            endSingleTimeCommands(ctx.device, ctx.graphicsQ, ctx.cmdPool, cmd);

            destroyBuffer(ctx.device, stV);
            destroyBuffer(ctx.device, stI);
        }
    }

    AllocatedBuffer uboBuf[VkContext::kMaxFrames]{};
    void *uboMap[VkContext::kMaxFrames]{};
    VkDescriptorSet uboSet[VkContext::kMaxFrames]{};

    for (uint32_t i = 0; i < VkContext::kMaxFrames; i++)
    {
        uboBuf[i] = createBuffer(ctx.phys, ctx.device, sizeof(GlobalUBO),
                                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(ctx.device, uboBuf[i].memory, 0, sizeof(GlobalUBO), 0, &uboMap[i]);

        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = gfxPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &uboSetLayout;
        vkAllocateDescriptorSets(ctx.device, &ai, &uboSet[i]);

        VkDescriptorBufferInfo bi{};
        bi.buffer = uboBuf[i].buffer;
        bi.offset = 0;
        bi.range = sizeof(GlobalUBO);

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = uboSet[i];
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(ctx.device, 1, &w, 0, nullptr);
    }

    // texture descriptor sets
    VkDescriptorSet texSet[2]{};
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = gfxPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &texSetLayout;

        VkDescriptorImageInfo fft{};
        fft.sampler = fftSampler;
        fft.imageView = texBCombined.view;
        fft.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo fftDetail{};
        fftDetail.sampler = fftSampler;
        fftDetail.imageView = texB0_1.view;
        fftDetail.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo hdr{};
        hdr.sampler = hdrSampler;
        hdr.imageView = hdrImg.view;
        hdr.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // scene color/depth
        VkDescriptorImageInfo scn{};
        scn.sampler = sceneColorSampler;
        scn.imageView = sceneColor.view;
        scn.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo sdepth{};
        sdepth.sampler = sceneDepthSampler;
        sdepth.imageView = sceneDepth.view;
        sdepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        for (int i = 0; i < 2; i++)
        {
            vkAllocateDescriptorSets(ctx.device, &ai, &texSet[i]);

            VkDescriptorImageInfo foam{};
            foam.sampler = foamSampler;
            foam.imageView = foamImg[i].view;
            foam.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorImageInfo wake{};
            wake.sampler = foamSampler;
            wake.imageView = foamImg[i].view;
            wake.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            std::array<VkWriteDescriptorSet, 7> wr{};
            wr[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[0].dstSet = texSet[i];
            wr[0].dstBinding = 0;
            wr[0].descriptorCount = 1;
            wr[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[0].pImageInfo = &fft;

            wr[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[1].dstSet = texSet[i];
            wr[1].dstBinding = 1;
            wr[1].descriptorCount = 1;
            wr[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[1].pImageInfo = &hdr;

            wr[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[2].dstSet = texSet[i];
            wr[2].dstBinding = 2;
            wr[2].descriptorCount = 1;
            wr[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[2].pImageInfo = &foam;

            wr[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[3].dstSet = texSet[i];
            wr[3].dstBinding = 3;
            wr[3].descriptorCount = 1;
            wr[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[3].pImageInfo = &scn;

            wr[4] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[4].dstSet = texSet[i];
            wr[4].dstBinding = 4;
            wr[4].descriptorCount = 1;
            wr[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[4].pImageInfo = &sdepth;

            wr[5] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[5].dstSet = texSet[i];
            wr[5].dstBinding = 5;
            wr[5].descriptorCount = 1;
            wr[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[5].pImageInfo = &fftDetail;

            wr[6] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[6].dstSet = texSet[i];
            wr[6].dstBinding = 6;
            wr[6].descriptorCount = 1;
            wr[6].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[6].pImageInfo = &wake;

            vkUpdateDescriptorSets(ctx.device, (uint32_t)wr.size(), wr.data(), 0, nullptr);
        }
    }

    // TAA descriptor sets & buffers
    VkDescriptorSet spraySet{};
    AllocatedBuffer taaUboBuf[VkContext::kMaxFrames]{};
    void *taaUboMap[VkContext::kMaxFrames]{};
    VkDescriptorSet taaSet[VkContext::kMaxFrames][2]{};
    VkDescriptorSet tonemapSet[2]{};
    VkDescriptorSet dsSpray{};

    // spray graphics set
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = gfxPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &spraySetLayout;
        vkAllocateDescriptorSets(ctx.device, &ai, &spraySet);

        VkDescriptorBufferInfo bi{};
        bi.buffer = sprayBuf.buffer;
        bi.offset = 0;
        bi.range = sprayBuf.size;

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = spraySet;
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        w.pBufferInfo = &bi;
        vkUpdateDescriptorSets(ctx.device, 1, &w, 0, nullptr);
    }

    // TAA UBOs + descriptor sets
    for (uint32_t fi = 0; fi < VkContext::kMaxFrames; ++fi)
    {
        taaUboBuf[fi] = createBuffer(ctx.phys, ctx.device, sizeof(TaaUBO),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(ctx.device, taaUboBuf[fi].memory, 0, sizeof(TaaUBO), 0, &taaUboMap[fi]);

        for (int h = 0; h < 2; ++h)
        {
            VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            ai.descriptorPool = gfxPool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts = &taaSetLayout;
            vkAllocateDescriptorSets(ctx.device, &ai, &taaSet[fi][h]);

            VkDescriptorBufferInfo ubi{};
            ubi.buffer = taaUboBuf[fi].buffer;
            ubi.offset = 0;
            ubi.range = sizeof(TaaUBO);

            VkDescriptorImageInfo curr{};
            curr.sampler = mainColorSampler;
            curr.imageView = mainColor.view;
            curr.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo dep{};
            dep.sampler = mainDepthSampler;
            dep.imageView = mainDepth.view;
            dep.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo hist{};
            hist.sampler = taaSampler;
            hist.imageView = taaHist[h].view;
            hist.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            std::array<VkWriteDescriptorSet, 4> wr{};
            wr[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[0].dstSet = taaSet[fi][h];
            wr[0].dstBinding = 0;
            wr[0].descriptorCount = 1;
            wr[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            wr[0].pBufferInfo = &ubi;

            wr[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[1].dstSet = taaSet[fi][h];
            wr[1].dstBinding = 1;
            wr[1].descriptorCount = 1;
            wr[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[1].pImageInfo = &curr;

            wr[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[2].dstSet = taaSet[fi][h];
            wr[2].dstBinding = 2;
            wr[2].descriptorCount = 1;
            wr[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[2].pImageInfo = &dep;

            wr[3] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            wr[3].dstSet = taaSet[fi][h];
            wr[3].dstBinding = 3;
            wr[3].descriptorCount = 1;
            wr[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            wr[3].pImageInfo = &hist;

            vkUpdateDescriptorSets(ctx.device, (uint32_t)wr.size(), wr.data(), 0, nullptr);
        }
    }

    // tonemap sets
    for (int h = 0; h < 2; ++h)
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = gfxPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &tonemapSetLayout;
        vkAllocateDescriptorSets(ctx.device, &ai, &tonemapSet[h]);

        VkDescriptorImageInfo ii{};
        ii.sampler = taaSampler;
        ii.imageView = taaHist[h].view;
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = tonemapSet[h];
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &ii;
        vkUpdateDescriptorSets(ctx.device, 1, &w, 0, nullptr);
    }

    // spray compute set
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = compPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &compSpraySetLayout;
        vkAllocateDescriptorSets(ctx.device, &ai, &dsSpray);

        VkDescriptorImageInfo fft{};
        fft.sampler = fftSampler;
        fft.imageView = texBCombined.view;
        fft.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo fftDetail{};
        fftDetail.sampler = fftSampler;
        fftDetail.imageView = texB0_1.view;
        fftDetail.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorBufferInfo biP{};
        biP.buffer = sprayBuf.buffer;
        biP.offset = 0;
        biP.range = sprayBuf.size;

        VkDescriptorBufferInfo biC{};
        biC.buffer = sprayCounter.buffer;
        biC.offset = 0;
        biC.range = sizeof(uint32_t);

        std::array<VkWriteDescriptorSet, 3> wr{};
        wr[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[0].dstSet = dsSpray;
        wr[0].dstBinding = 0;
        wr[0].descriptorCount = 1;
        wr[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wr[0].pImageInfo = &fft;

        wr[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[1].dstSet = dsSpray;
        wr[1].dstBinding = 1;
        wr[1].descriptorCount = 1;
        wr[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wr[1].pBufferInfo = &biP;

        wr[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[2].dstSet = dsSpray;
        wr[2].dstBinding = 2;
        wr[2].descriptorCount = 1;
        wr[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        wr[2].pBufferInfo = &biC;

        vkUpdateDescriptorSets(ctx.device, (uint32_t)wr.size(), wr.data(), 0, nullptr);
    }

    // compute descriptor sets
    VkDescriptorSet dsSpectrum0{}, dsBuild0{}, dsRows0{}, dsCols0{};
    VkDescriptorSet dsSpectrum1{}, dsBuild1{}, dsRows1{}, dsCols1{};
    VkDescriptorSet dsCombine{};
    VkDescriptorSet dsFoam[2]{};
    auto allocCompSet = [&](VkDescriptorSetLayout layout, VkDescriptorSet &out)
    {
        VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        ai.descriptorPool = compPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts = &layout;
        vkAllocateDescriptorSets(ctx.device, &ai, &out);
    };

    // two FFT bands
    allocCompSet(compSpectrumSetLayout, dsSpectrum0);
    allocCompSet(comp2ImgSetLayout, dsBuild0);
    allocCompSet(comp2ImgSetLayout, dsRows0);
    allocCompSet(comp2ImgSetLayout, dsCols0);

    allocCompSet(compSpectrumSetLayout, dsSpectrum1);
    allocCompSet(comp2ImgSetLayout, dsBuild1);
    allocCompSet(comp2ImgSetLayout, dsRows1);
    allocCompSet(comp2ImgSetLayout, dsCols1);

    allocCompSet(comp3ImgSetLayout, dsCombine);

    allocCompSet(compFoamSetLayout, dsFoam[0]);
    allocCompSet(compFoamSetLayout, dsFoam[1]);
    auto writeSpectrum = [&](VkDescriptorSet set, VkImageView outView)
    {
        VkDescriptorImageInfo outH{};
        outH.imageView = outView;
        outH.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = set;
        w.dstBinding = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w.pImageInfo = &outH;
        vkUpdateDescriptorSets(ctx.device, 1, &w, 0, nullptr);
    };

    writeSpectrum(dsSpectrum0, texH0.view);
    writeSpectrum(dsSpectrum1, texH1.view);

    auto write2 = [&](VkDescriptorSet set, VkImageView src, VkImageView dst)
    {
        VkDescriptorImageInfo a{};
        a.imageView = src;
        a.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo b{};
        b.imageView = dst;
        b.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet w0{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w0.dstSet = set;
        w0.dstBinding = 0;
        w0.descriptorCount = 1;
        w0.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w0.pImageInfo = &a;
        VkWriteDescriptorSet w1{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w1.dstSet = set;
        w1.dstBinding = 1;
        w1.descriptorCount = 1;
        w1.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w1.pImageInfo = &b;
        VkWriteDescriptorSet ws[2]{w0, w1};
        vkUpdateDescriptorSets(ctx.device, 2, ws, 0, nullptr);
    };

    // 0 swell
    write2(dsBuild0, texH0.view, texB0_0.view);
    write2(dsRows0, texB0_0.view, texB1_0.view);
    write2(dsCols0, texB1_0.view, texB0_0.view);

    // 1 wind
    write2(dsBuild1, texH1.view, texB0_1.view);
    write2(dsRows1, texB0_1.view, texB1_1.view);
    write2(dsCols1, texB1_1.view, texB0_1.view);

    // combine
    {
        VkDescriptorImageInfo a{};
        a.imageView = texB0_0.view;
        a.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo b{};
        b.imageView = texB0_1.view;
        b.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo o{};
        o.imageView = texBCombined.view;
        o.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        std::array<VkWriteDescriptorSet, 3> wr{};
        wr[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[0].dstSet = dsCombine;
        wr[0].dstBinding = 0;
        wr[0].descriptorCount = 1;
        wr[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        wr[0].pImageInfo = &a;

        wr[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[1].dstSet = dsCombine;
        wr[1].dstBinding = 1;
        wr[1].descriptorCount = 1;
        wr[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        wr[1].pImageInfo = &b;

        wr[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        wr[2].dstSet = dsCombine;
        wr[2].dstBinding = 2;
        wr[2].descriptorCount = 1;
        wr[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        wr[2].pImageInfo = &o;

        vkUpdateDescriptorSets(ctx.device, (uint32_t)wr.size(), wr.data(), 0, nullptr);
    }

    auto writeFoam = [&](VkDescriptorSet set, VkImageView prevFoam, VkImageView outFoam)
    {
        VkDescriptorImageInfo fft{};
        fft.sampler = fftSampler;
        fft.imageView = texBCombined.view;
        fft.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo fftDetail{};
        fftDetail.sampler = fftSampler;
        fftDetail.imageView = texB0_1.view;
        fftDetail.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo prev{};
        prev.sampler = foamSampler;
        prev.imageView = prevFoam;
        prev.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkDescriptorImageInfo out{};
        out.sampler = VK_NULL_HANDLE;
        out.imageView = outFoam;
        out.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet w0{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w0.dstSet = set;
        w0.dstBinding = 0;
        w0.descriptorCount = 1;
        w0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w0.pImageInfo = &fft;
        VkWriteDescriptorSet w1{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w1.dstSet = set;
        w1.dstBinding = 1;
        w1.descriptorCount = 1;
        w1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w1.pImageInfo = &prev;
        VkWriteDescriptorSet w2{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w2.dstSet = set;
        w2.dstBinding = 2;
        w2.descriptorCount = 1;
        w2.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w2.pImageInfo = &out;

        VkWriteDescriptorSet ws[3]{w0, w1, w2};
        vkUpdateDescriptorSets(ctx.device, 3, ws, 0, nullptr);
    };

    writeFoam(dsFoam[0], foamImg[0].view, foamImg[1].view);
    writeFoam(dsFoam[1], foamImg[1].view, foamImg[0].view);

    float time = 0.0f;
    float dbgTimer = 0.0f;
    uint32_t foamParity = 0;

    // init boat, still under maybe put it more infront
    if (glm::length(gBoatPos) < 0.001f)
    {
        gBoatYaw = glm::radians(90.0f);
        glm::vec2 camW = worldOrigin + glm::vec2(cameraPos.x, cameraPos.z);
        glm::vec2 fwd(std::cos(gBoatYaw), std::sin(gBoatYaw));
        gBoatPos = camW + fwd * 25.0f;
        if (!infiniteOcean)
        {
            const float margin = std::max(18.0f, gDuckScaleMeters * 0.8f);
            glm::vec2 minW = worldOrigin + glm::vec2(-512.0f * 0.5f + margin, -512.0f * 0.5f + margin);
            glm::vec2 maxW = worldOrigin + glm::vec2(512.0f * 0.5f - margin, 512.0f * 0.5f - margin);
            gBoatPos = glm::clamp(gBoatPos, minW, maxW);
        }
    }

    VkExtent2D lastExtent = ctx.swapExtent;
    VkRenderPass lastSwapRenderPass = ctx.renderPass;

    glm::mat4 prevVP = glm::mat4(1.0f);
    bool hasPrevVP = false;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window, deltaTime);
        glfwPollEvents();

        time += deltaTime * gWaveSpeed;

        // snap
        glm::vec2 camXZ(cameraPos.x, cameraPos.z);
        glm::vec2 snap = glm::floor(camXZ / PATCH_SIZE) * PATCH_SIZE;
        if (snap.x != 0.0f || snap.y != 0.0f)
        {
            cameraPos.x -= snap.x;
            cameraPos.z -= snap.y;
            worldOrigin += snap;
        }

        uint32_t imageIndex = 0;
        VkCommandBuffer cmd = ctx.beginFrame(imageIndex);
        if (cmd == VK_NULL_HANDLE)
            continue;

        if (ctx.renderPass != lastSwapRenderPass)
        {
            vkDeviceWaitIdle(ctx.device);
            if (tonemapPipe)
                vkDestroyPipeline(ctx.device, tonemapPipe, nullptr);
            tonemapPipe = createGraphicsPipeline(ctx.device, ctx.renderPass, tonemapLayout, ctx.swapExtent,
                                                 spv("fullscreen.vert.spv"), spv("tonemap.frag.spv"),
                                                 false,
                                                 false, VK_COMPARE_OP_ALWAYS,
                                                 VK_POLYGON_MODE_FILL,
                                                 VK_CULL_MODE_NONE,
                                                 false);
            lastSwapRenderPass = ctx.renderPass;
        }

        if (ctx.swapExtent.width != lastExtent.width || ctx.swapExtent.height != lastExtent.height)
        {
            vkDeviceWaitIdle(ctx.device);
            rebuildSceneTargets(ctx.swapExtent);
            rebuildMainTargets(ctx.swapExtent);
            rebuildTaaTargets(ctx.swapExtent);

            VkDescriptorImageInfo scn{};
            scn.sampler = sceneColorSampler;
            scn.imageView = sceneColor.view;
            scn.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkDescriptorImageInfo sdepth{};
            sdepth.sampler = sceneDepthSampler;
            sdepth.imageView = sceneDepth.view;
            sdepth.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

            for (int i = 0; i < 2; i++)
            {
                std::array<VkWriteDescriptorSet, 2> wr{};
                wr[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                wr[0].dstSet = texSet[i];
                wr[0].dstBinding = 3;
                wr[0].descriptorCount = 1;
                wr[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                wr[0].pImageInfo = &scn;

                wr[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                wr[1].dstSet = texSet[i];
                wr[1].dstBinding = 4;
                wr[1].descriptorCount = 1;
                wr[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                wr[1].pImageInfo = &sdepth;

                vkUpdateDescriptorSets(ctx.device, (uint32_t)wr.size(), wr.data(), 0, nullptr);
            }

            // Update TAA/tonemap
            for (uint32_t fi = 0; fi < VkContext::kMaxFrames; ++fi)
            {
                for (int h = 0; h < 2; ++h)
                {
                    VkDescriptorImageInfo curr{};
                    curr.sampler = mainColorSampler;
                    curr.imageView = mainColor.view;
                    curr.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    VkDescriptorImageInfo dep{};
                    dep.sampler = mainDepthSampler;
                    dep.imageView = mainDepth.view;
                    dep.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

                    VkDescriptorImageInfo hist{};
                    hist.sampler = taaSampler;
                    hist.imageView = taaHist[h].view;
                    hist.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    std::array<VkWriteDescriptorSet, 3> wrT{};
                    wrT[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    wrT[0].dstSet = taaSet[fi][h];
                    wrT[0].dstBinding = 1;
                    wrT[0].descriptorCount = 1;
                    wrT[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    wrT[0].pImageInfo = &curr;

                    wrT[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    wrT[1].dstSet = taaSet[fi][h];
                    wrT[1].dstBinding = 2;
                    wrT[1].descriptorCount = 1;
                    wrT[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    wrT[1].pImageInfo = &dep;

                    wrT[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                    wrT[2].dstSet = taaSet[fi][h];
                    wrT[2].dstBinding = 3;
                    wrT[2].descriptorCount = 1;
                    wrT[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                    wrT[2].pImageInfo = &hist;

                    vkUpdateDescriptorSets(ctx.device, (uint32_t)wrT.size(), wrT.data(), 0, nullptr);
                }
            }

            for (int h = 0; h < 2; ++h)
            {
                VkDescriptorImageInfo ii{};
                ii.sampler = taaSampler;
                ii.imageView = taaHist[h].view;
                ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
                w.dstSet = tonemapSet[h];
                w.dstBinding = 0;
                w.descriptorCount = 1;
                w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                w.pImageInfo = &ii;
                vkUpdateDescriptorSets(ctx.device, 1, &w, 0, nullptr);
            }

            lastExtent = ctx.swapExtent;
        }

        // FFT chain
        float invN = 1.0f / float(FREQ_SIZE);
        struct alignas(8)
        {
            float invN;
            float finalScale;
        } ipc{};
        ipc.invN = invN;
        ipc.finalScale = 25.0f;

        auto runFFTBand = [&](VkDescriptorSet dsSpec, VkDescriptorSet dsB, VkDescriptorSet dsR, VkDescriptorSet dsC,
                              AllocatedImage &H, AllocatedImage &B0, AllocatedImage &B1,
                              float windX, float windY, float amp, float windSpeed,
                              float patchSize, float seed)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csSpectrum);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compSpectrumLayout, 0, 1, &dsSpec, 0, nullptr);

            struct alignas(16)
            {
                float t;
                float windX;
                float windY;
                float amp;
                float windSpeed;
                float pad[3];
            } sp{};
            sp.t = time;
            sp.windX = windX;
            sp.windY = windY;
            sp.amp = amp;
            sp.windSpeed = windSpeed;
            sp.pad[0] = patchSize;
            sp.pad[1] = seed;
            vkCmdPushConstants(cmd, compSpectrumLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 32, &sp);
            vkCmdDispatch(cmd, (uint32_t)((FREQ_SIZE + 15) / 16), (uint32_t)((FREQ_SIZE + 15) / 16), 1);

            imageBarrierGeneral(cmd, H.image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                1, 1);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csBuild);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compBuildLayout, 0, 1, &dsB, 0, nullptr);
            vkCmdDispatch(cmd, (uint32_t)(((3 * FREQ_SIZE) + 15) / 16), (uint32_t)((FREQ_SIZE + 15) / 16), 1);

            imageBarrierGeneral(cmd, B0.image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                1, 1);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csRows);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compIfftLayout, 0, 1, &dsR, 0, nullptr);
            vkCmdPushConstants(cmd, compIfftLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ipc), &ipc);
            vkCmdDispatch(cmd, (uint32_t)FREQ_SIZE, 3, 1);

            imageBarrierGeneral(cmd, B1.image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                1, 1);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csCols);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compIfftLayout, 0, 1, &dsC, 0, nullptr);
            vkCmdPushConstants(cmd, compIfftLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ipc), &ipc);
            vkCmdDispatch(cmd, (uint32_t)FREQ_SIZE, 3, 1);

            imageBarrierGeneral(cmd, B0.image, VK_IMAGE_ASPECT_COLOR_BIT,
                                VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                1, 1);
        };

        // 0 swell
        runFFTBand(dsSpectrum0, dsBuild0, dsRows0, dsCols0, texH0, texB0_0, texB1_0,
                   0.8f, 0.2f, 0.0018f, 38.0f,
                   PATCH_SIZE, 1337.0f);

        // 1 wind
        runFFTBand(dsSpectrum1, dsBuild1, dsRows1, dsCols1, texH1, texB0_1, texB1_1,
                   1.0f, 0.0f, 0.0030f, 22.0f,
                   PATCH_SIZE, 424242.0f);

        // combine
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csCombine);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compCombineLayout, 0, 1, &dsCombine, 0, nullptr);
        struct alignas(16)
        {
            float windDisp;
            float pad[3];
        } cpc{};
        cpc.windDisp = 0.35f;
        vkCmdPushConstants(cmd, compCombineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, &cpc);
        vkCmdDispatch(cmd, (uint32_t)(((3 * FREQ_SIZE) + 15) / 16), (uint32_t)((FREQ_SIZE + 15) / 16), 1);

        // make combination visible
        imageBarrierGeneral(cmd, texBCombined.image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            1, 1);

        // wind visibility
        imageBarrierGeneral(cmd, texB0_1.image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            1, 1);

        uint32_t foamRead = foamParity;
        uint32_t foamWrite = 1u - foamRead;

        imageBarrierGeneral(cmd, texBCombined.image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            1, 1);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csFoam);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compFoamLayout, 0, 1, &dsFoam[foamRead], 0, nullptr);

        struct alignas(16)
        {
            float dt;
            float patchSize;
            float choppy;
            float flowScale;
            float decay;
            float inject;
            float slope0;
            float slope1;
            float fold0;
            float fold1;
            float streak;
            float spray;
        } fpc{};
        fpc.dt = std::min(deltaTime, 0.050f);
        fpc.patchSize = PATCH_SIZE;
        fpc.choppy = gChoppy;
        fpc.flowScale = 0.75f;
        fpc.decay = 0.22f;
        fpc.inject = 1.60f;
        fpc.slope0 = 0.09f;
        fpc.slope1 = 0.30f;
        fpc.fold0 = 0.00f;
        fpc.fold1 = 0.60f;
        fpc.streak = 1.00f;
        fpc.spray = 0.0f;

        vkCmdPushConstants(cmd, compFoamLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 48, &fpc);
        vkCmdDispatch(cmd, (uint32_t)((FREQ_SIZE + 15) / 16), (uint32_t)((FREQ_SIZE + 15) / 16), 1);

        imageBarrierGeneral(cmd, foamImg[foamWrite].image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            1, 1);

        // make texB0 visible
        imageBarrierGeneral(cmd, texBCombined.image, VK_IMAGE_ASPECT_COLOR_BIT,
                            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                            1, 1);

        // update
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csSprayUpdate);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compSprayUpdateLayout, 0, 1, &dsSpray, 0, nullptr);
        struct alignas(16)
        {
            float dt;
            float drag;
            float gravity;
            float pad;
        } upc{};
        upc.dt = std::min(deltaTime, 0.050f);
        upc.drag = 1.5f;
        upc.gravity = -9.8f;
        vkCmdPushConstants(cmd, compSprayUpdateLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, &upc);
        vkCmdDispatch(cmd, (MAX_PARTICLES + 255) / 256, 1, 1);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, csSpraySpawn);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compSpraySpawnLayout, 0, 1, &dsSpray, 0, nullptr);
        struct alignas(16)
        {
            float dt;
            float patchSize;
            float choppy;
            float heightScale;
            float spawnArea;
            float windX;
            float windY;
            float spawnRate;
            float fold0;
            float fold1;
            float slope0;
            float slope1;
            float baseLife;
            float vUp;
            float vSide;
            float pad;
        } spc{};
        spc.dt = std::min(deltaTime, 0.050f);
        spc.patchSize = PATCH_SIZE;
        spc.choppy = gChoppy;
        spc.heightScale = gHeightScale;
        spc.spawnArea = PATCH_SIZE * 2.25f;
        spc.windX = 1.0f;
        spc.windY = 0.0f;
        spc.spawnRate = 8.0f;
        spc.fold0 = 0.0f;
        spc.fold1 = 0.6f;
        spc.slope0 = 0.10f;
        spc.slope1 = 0.35f;
        spc.baseLife = 1.15f;
        spc.vUp = 8.5f;
        spc.vSide = 4.0f;
        vkCmdPushConstants(cmd, compSpraySpawnLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 64, &spc);
        vkCmdDispatch(cmd, (128 + 15) / 16, (128 + 15) / 16, 1);

        // make spray buffer visible to vertex shader
        bufferBarrier(cmd, sprayBuf.buffer,
                      VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                      VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);

        // updaet UBO
        GlobalUBO ubo{};
        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        const float nearZ = 0.01f;
        const float farZ = 8000.0f;
        glm::mat4 proj = glm::perspective(glm::radians(fov),
                                          (float)ctx.swapExtent.width / (float)ctx.swapExtent.height,
                                          nearZ, farZ);
        proj[1][1] *= -1.0f;

        glm::mat4 currVP = proj * view;
        if (!hasPrevVP)
        {
            prevVP = currVP;
            hasPrevVP = true;
        }
        TaaUBO taau{};
        taau.invCurrVP = glm::inverse(currVP);
        taau.prevVP = prevVP;
        taau.params = glm::vec4(1.0f / float(ctx.swapExtent.width),
                                1.0f / float(ctx.swapExtent.height),
                                0.10f, 0.0f);
        std::memcpy(taaUboMap[ctx.frameIndex], &taau, sizeof(taau));

        ubo.view = view;
        ubo.proj = proj;
        ubo.cameraPos_time = glm::vec4(cameraPos, time);
        ubo.wave0 = glm::vec4(PATCH_SIZE, gHeightScale, gChoppy, gSwellAmp);
        ubo.worldOrigin_pad = glm::vec4(worldOrigin.x, worldOrigin.y, 0.0f, 0.0f);
        ubo.wave1 = glm::vec4(gSwellSpeed, dayNight, gExposure, envMaxMip);
        ubo.debug = glm::ivec4(shaderDebug, 0, 0, 0);
        ubo.screen = glm::vec4(1.0f / float(ctx.swapExtent.width),
                               1.0f / float(ctx.swapExtent.height),
                               nearZ, farZ);

        // boat parameters for water.frag
        ubo.boat0 = glm::vec4(gBoatPos.x, gBoatPos.y, gBoatYaw, 0.0f);
        ubo.boat1 = glm::vec4(std::abs(gBoatSpeed), gBoatLen, gBoatWid, gBoatDraft);

        std::memcpy(uboMap[ctx.frameIndex], &ubo, sizeof(ubo));

        // offscreen pass for water.frag
        if (sceneFramebuffer && hdrImg.image)
        {
            VkClearValue sclr[2]{};
            sclr[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
            sclr[1].depthStencil = {1.0f, 0};

            VkRenderPassBeginInfo sbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            sbi.renderPass = sceneRenderPass;
            sbi.framebuffer = sceneFramebuffer;
            sbi.renderArea.extent = ctx.swapExtent;
            sbi.clearValueCount = 2;
            sbi.pClearValues = sclr;

            vkCmdBeginRenderPass(cmd, &sbi, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport svp{};
            svp.x = 0;
            svp.y = 0;
            svp.width = (float)ctx.swapExtent.width;
            svp.height = (float)ctx.swapExtent.height;
            svp.minDepth = 0.0f;
            svp.maxDepth = 1.0f;
            VkRect2D ssc{};
            ssc.extent = ctx.swapExtent;

            vkCmdSetViewport(cmd, 0, 1, &svp);
            vkCmdSetScissor(cmd, 0, 1, &ssc);

            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sceneSkyPipe);
            VkDescriptorSet skySets[2] = {uboSet[ctx.frameIndex], texSet[foamWrite]};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyLayout, 0, 2, skySets, 0, nullptr);
            vkCmdDraw(cmd, 36, 1, 0, 0);
            vkCmdEndRenderPass(cmd);
        }

        // main HDR pass
        VkClearValue mclr[2]{};
        mclr[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        mclr[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo mbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        mbi.renderPass = mainRenderPass;
        mbi.framebuffer = mainFramebuffer;
        mbi.renderArea.extent = ctx.swapExtent;
        mbi.clearValueCount = 2;
        mbi.pClearValues = mclr;

        vkCmdBeginRenderPass(cmd, &mbi, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport vp{};
        vp.x = 0;
        vp.y = 0;
        vp.width = (float)ctx.swapExtent.width;
        vp.height = (float)ctx.swapExtent.height;
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        VkRect2D sc{};
        sc.extent = ctx.swapExtent;

        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);

        if (hdrImg.image)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyMainPipe);
            VkDescriptorSet skySets[2] = {uboSet[ctx.frameIndex], texSet[foamWrite]};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, skyLayout, 0, 2, skySets, 0, nullptr);
            vkCmdDraw(cmd, 36, 1, 0, 0);
        }

        VkPipeline useWater = (wireframe && waterLine) ? waterLine : waterFill;
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, useWater);

        VkDescriptorSet sets[2] = {uboSet[ctx.frameIndex], texSet[foamWrite]};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, waterLayout, 0, 2, sets, 0, nullptr);

        auto bindMesh = [&](const WaterMesh &m)
        {
            VkBuffer vb = m.vbo.buffer;
            VkDeviceSize off = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vb, &off);
            vkCmdBindIndexBuffer(cmd, m.ibo.buffer, 0, VK_INDEX_TYPE_UINT32);
        };

        const WaterMesh *curMesh = nullptr;

        auto drawTile = [&](const glm::vec2 &tileOrigin, const WaterMesh &m)
        {
            if (curMesh != &m)
            {
                bindMesh(m);
                curMesh = &m;
            }
            WaterPush pc{};
            pc.worldOffset = tileOrigin;
            vkCmdPushConstants(cmd, waterLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(WaterPush), &pc);
            vkCmdDrawIndexed(cmd, m.indexCount, 1, 0, 0, 0);
        };

        if (!infiniteOcean)
        {
            drawTile(worldOrigin, meshHi);
        }
        else
        {
            float rr = float(oceanRadius) + 0.5f;
            float rr2 = rr * rr;

            for (int tz = -oceanRadius; tz <= oceanRadius; ++tz)
            {
                for (int tx = -oceanRadius; tx <= oceanRadius; ++tx)
                {
                    if (float(tx * tx + tz * tz) > rr2)
                        continue;

                    int ring = std::max(std::abs(tx), std::abs(tz));
                    const WaterMesh *m = (ring <= lod0Radius) ? &meshHi : (ring <= lod1Radius) ? &meshMid
                                                                                               : &meshLo;

                    glm::vec2 tileOrigin = worldOrigin + glm::vec2((float)tx * PATCH_SIZE, (float)tz * PATCH_SIZE);
                    drawTile(tileOrigin, *m);
                }
            }
        }

        // boat float on waves
        if (gBoatEnabled && boatPipe)
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, boatPipe);
            VkDescriptorSet bSets[2] = {uboSet[ctx.frameIndex], texSet[foamWrite]};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, boatLayout, 0, 2, bSets, 0, nullptr);

            BoatPush bpc{};
            bpc.boat0 = glm::vec4(gBoatPos.x, gBoatPos.y, gBoatYaw, gDuckScaleMeters);
            bpc.boat1 = glm::vec4(gBoatLen, gBoatWid, gBoatH, gBoatDraft);
            vkCmdPushConstants(cmd, boatLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(BoatPush), &bpc);

            VkDeviceSize zOff = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &duckMesh.vbo.buffer, &zOff);
            vkCmdBindIndexBuffer(cmd, duckMesh.ibo.buffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, duckMesh.indexCount, 1, 0, 0, 0);
        }

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sprayPipe);
        VkDescriptorSet sprSets[2] = {uboSet[ctx.frameIndex], spraySet};
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, sprayLayout, 0, 2, sprSets, 0, nullptr);
        vkCmdDraw(cmd, 6, MAX_PARTICLES, 0, 0);

        vkCmdEndRenderPass(cmd);

        uint32_t taaRead = taaParity;
        uint32_t taaWrite = 1u - taaRead;

        VkClearValue tclr{};
        tclr.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo tbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        tbi.renderPass = taaRenderPass;
        tbi.framebuffer = taaFB[taaWrite];
        tbi.renderArea.extent = ctx.swapExtent;
        tbi.clearValueCount = 1;
        tbi.pClearValues = &tclr;
        vkCmdBeginRenderPass(cmd, &tbi, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taaPipe);
        VkDescriptorSet taaDS = taaSet[ctx.frameIndex][taaRead];
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, taaLayout, 0, 1, &taaDS, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);

        VkClearValue clears[2]{};
        clears[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        clears[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rbi{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        rbi.renderPass = ctx.renderPass;
        rbi.framebuffer = ctx.framebuffers[imageIndex];
        rbi.renderArea.extent = ctx.swapExtent;
        rbi.clearValueCount = 2;
        rbi.pClearValues = clears;
        vkCmdBeginRenderPass(cmd, &rbi, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdSetViewport(cmd, 0, 1, &vp);
        vkCmdSetScissor(cmd, 0, 1, &sc);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipe);
        VkDescriptorSet tm = tonemapSet[taaWrite];
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapLayout, 0, 1, &tm, 0, nullptr);
        float toneExposure = 1.0f;
        vkCmdPushConstants(cmd, tonemapLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, 4, &toneExposure);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRenderPass(cmd);

        taaParity = taaWrite;

        ctx.endFrame(imageIndex);
        foamParity = foamWrite;
        prevVP = currVP;

        dbgTimer += deltaTime;
        if (dbgTimer > 1.0f)
        {
            dbgTimer = 0.0f;
        }
    }

    ctx.waitIdle();

    // clean
    if (waterFill)
        vkDestroyPipeline(ctx.device, waterFill, nullptr);
    if (waterLine)
        vkDestroyPipeline(ctx.device, waterLine, nullptr);
    if (skyMainPipe)
        vkDestroyPipeline(ctx.device, skyMainPipe, nullptr);
    if (boatPipe)
        vkDestroyPipeline(ctx.device, boatPipe, nullptr);
    if (sprayPipe)
        vkDestroyPipeline(ctx.device, sprayPipe, nullptr);
    if (taaPipe)
        vkDestroyPipeline(ctx.device, taaPipe, nullptr);
    if (tonemapPipe)
        vkDestroyPipeline(ctx.device, tonemapPipe, nullptr);
    if (sceneSkyPipe)
        vkDestroyPipeline(ctx.device, sceneSkyPipe, nullptr);
    vkDestroyPipeline(ctx.device, csSpectrum, nullptr);
    vkDestroyPipeline(ctx.device, csBuild, nullptr);
    vkDestroyPipeline(ctx.device, csRows, nullptr);
    vkDestroyPipeline(ctx.device, csCols, nullptr);
    vkDestroyPipeline(ctx.device, csCombine, nullptr);
    vkDestroyPipeline(ctx.device, csFoam, nullptr);
    if (csSprayUpdate)
        vkDestroyPipeline(ctx.device, csSprayUpdate, nullptr);
    if (csSpraySpawn)
        vkDestroyPipeline(ctx.device, csSpraySpawn, nullptr);

    vkDestroyPipelineLayout(ctx.device, waterLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, skyLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, boatLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, compSpectrumLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, compBuildLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, compIfftLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, compCombineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, compFoamLayout, nullptr);
    if (compSprayUpdateLayout)
        vkDestroyPipelineLayout(ctx.device, compSprayUpdateLayout, nullptr);
    if (compSpraySpawnLayout)
        vkDestroyPipelineLayout(ctx.device, compSpraySpawnLayout, nullptr);
    if (sprayLayout)
        vkDestroyPipelineLayout(ctx.device, sprayLayout, nullptr);
    if (taaLayout)
        vkDestroyPipelineLayout(ctx.device, taaLayout, nullptr);
    if (tonemapLayout)
        vkDestroyPipelineLayout(ctx.device, tonemapLayout, nullptr);

    vkDestroyDescriptorSetLayout(ctx.device, uboSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, texSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, compSpectrumSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, comp2ImgSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, comp3ImgSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, compFoamSetLayout, nullptr);
    if (compSpraySetLayout)
        vkDestroyDescriptorSetLayout(ctx.device, compSpraySetLayout, nullptr);
    if (spraySetLayout)
        vkDestroyDescriptorSetLayout(ctx.device, spraySetLayout, nullptr);
    if (taaSetLayout)
        vkDestroyDescriptorSetLayout(ctx.device, taaSetLayout, nullptr);
    if (tonemapSetLayout)
        vkDestroyDescriptorSetLayout(ctx.device, tonemapSetLayout, nullptr);

    vkDestroyDescriptorPool(ctx.device, gfxPool, nullptr);
    vkDestroyDescriptorPool(ctx.device, compPool, nullptr);

    for (uint32_t i = 0; i < VkContext::kMaxFrames; i++)
    {
        if (uboMap[i])
            vkUnmapMemory(ctx.device, uboBuf[i].memory);
        destroyBuffer(ctx.device, uboBuf[i]);

        if (taaUboMap[i])
            vkUnmapMemory(ctx.device, taaUboBuf[i].memory);
        destroyBuffer(ctx.device, taaUboBuf[i]);
    }

    destroyBuffer(ctx.device, meshHi.vbo);
    destroyBuffer(ctx.device, meshHi.ibo);
    destroyBuffer(ctx.device, meshMid.vbo);
    destroyBuffer(ctx.device, meshMid.ibo);
    destroyBuffer(ctx.device, meshLo.vbo);
    destroyBuffer(ctx.device, meshLo.ibo);

    if (fftSampler)
        vkDestroySampler(ctx.device, fftSampler, nullptr);

    if (hdrSampler)
        vkDestroySampler(ctx.device, hdrSampler, nullptr);
    if (hdrImg.image)
        destroyImage(ctx.device, hdrImg);

    if (envCubeSampler)
        vkDestroySampler(ctx.device, envCubeSampler, nullptr);
    if (envCube.image)
        destroyImage(ctx.device, envCube);

    if (sceneColorSampler)
        vkDestroySampler(ctx.device, sceneColorSampler, nullptr);
    if (sceneDepthSampler)
        vkDestroySampler(ctx.device, sceneDepthSampler, nullptr);
    destroySceneTargets();
    if (sceneRenderPass)
        vkDestroyRenderPass(ctx.device, sceneRenderPass, nullptr);

    if (mainColorSampler)
        vkDestroySampler(ctx.device, mainColorSampler, nullptr);
    if (mainDepthSampler)
        vkDestroySampler(ctx.device, mainDepthSampler, nullptr);
    destroyMainTargets();
    if (mainRenderPass)
        vkDestroyRenderPass(ctx.device, mainRenderPass, nullptr);

    if (taaSampler)
        vkDestroySampler(ctx.device, taaSampler, nullptr);
    destroyTaaTargets();
    if (taaRenderPass)
        vkDestroyRenderPass(ctx.device, taaRenderPass, nullptr);

    destroyImage(ctx.device, texH0);
    destroyImage(ctx.device, texB0_0);
    destroyImage(ctx.device, texB1_0);
    destroyImage(ctx.device, texH1);
    destroyImage(ctx.device, texB0_1);
    destroyImage(ctx.device, texB1_1);
    destroyImage(ctx.device, texBCombined);
    destroyImage(ctx.device, foamImg[0]);
    destroyImage(ctx.device, foamImg[1]);

    destroyBuffer(ctx.device, sprayBuf);
    destroyBuffer(ctx.device, sprayCounter);

    ctx.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
