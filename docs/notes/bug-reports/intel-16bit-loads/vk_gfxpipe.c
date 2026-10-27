/* vk_gfxpipe.c - create one graphics pipeline from vertex + fragment SPIR-V files, nothing else.
 *
 *   vk_gfxpipe <vert.spv> <frag.spv> [gpu-name-substring]
 *
 * Vulkan 1.3 device with dynamicRendering, bufferDeviceAddress, scalarBlockLayout, shaderInt8/16/64,
 * storageBuffer8/16BitAccess, shaderDrawParameters enabled. Pipeline layout: set 0 binding 0 uniform
 * buffer, set 1 binding 0 storage buffer (all graphics stages). Dynamic rendering with one
 * B8G8R8A8_SRGB color attachment and D32_SFLOAT depth, no vertex input, dynamic viewport.
 *
 * Build (Windows): cl /nologo /W3 vk_gfxpipe.c /I"%VULKAN_SDK%\Include" /link /LIBPATH:"%VULKAN_SDK%\Lib" vulkan-1.lib
 * Build (Linux):   cc -o vk_gfxpipe vk_gfxpipe.c -lvulkan
 */
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { VkResult r_ = (x); if (r_ != VK_SUCCESS) { \
	fprintf(stderr, "%s failed: %d\n", #x, (int)r_); exit(1); } } while (0)

static uint32_t *readFile(const char *path, size_t *size)
{
	FILE *f = fopen(path, "rb");
	if (!f) { perror(path); exit(1); }
	fseek(f, 0, SEEK_END);
	long s = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint32_t *code = malloc((size_t)s);
	if (s <= 0 || s % 4 || fread(code, 1, (size_t)s, f) != (size_t)s) {
		fprintf(stderr, "bad SPIR-V file %s\n", path);
		exit(1);
	}
	fclose(f);
	*size = (size_t)s;
	return code;
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s <vert.spv> <frag.spv> [gpu-name-substring]\n", argv[0]);
		return 2;
	}
	size_t vs_size, fs_size;
	uint32_t *vs_code = readFile(argv[1], &vs_size);
	uint32_t *fs_code = readFile(argv[2], &fs_size);

	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.apiVersion = VK_API_VERSION_1_3;
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	VkInstance inst;
	CHECK(vkCreateInstance(&ici, NULL, &inst));

	uint32_t n = 16;
	VkPhysicalDevice pds[16], pd = VK_NULL_HANDLE;
	vkEnumeratePhysicalDevices(inst, &n, pds);
	for (uint32_t i = 0; i < n; i++) {
		VkPhysicalDeviceDriverProperties drv = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES };
		VkPhysicalDeviceProperties2 p = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &drv };
		vkGetPhysicalDeviceProperties2(pds[i], &p);
		int pick = !pd && (argc < 4 || strstr(p.properties.deviceName, argv[3]));
		printf("%s %s | %s | %s | driverVersion 0x%x\n", pick ? "*" : " ", p.properties.deviceName,
			drv.driverName, drv.driverInfo, p.properties.driverVersion);
		if (pick)
			pd = pds[i];
	}
	if (!pd) { fprintf(stderr, "no matching device\n"); return 1; }

	uint32_t nq = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, NULL);
	VkQueueFamilyProperties qf[32];
	if (nq > 32) nq = 32;
	vkGetPhysicalDeviceQueueFamilyProperties(pd, &nq, qf);
	uint32_t family = 0;
	while (family < nq && !(qf[family].queueFlags & VK_QUEUE_GRAPHICS_BIT))
		family++;

	float prio = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qci.queueFamilyIndex = family;
	qci.queueCount = 1;
	qci.pQueuePriorities = &prio;

	VkPhysicalDeviceVulkan13Features f13 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
	f13.dynamicRendering = VK_TRUE;
	f13.synchronization2 = VK_TRUE;
	VkPhysicalDeviceVulkan12Features f12 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, &f13 };
	f12.bufferDeviceAddress = VK_TRUE;
	f12.scalarBlockLayout = VK_TRUE;
	f12.shaderInt8 = VK_TRUE;
	f12.storageBuffer8BitAccess = VK_TRUE;
	f12.uniformAndStorageBuffer8BitAccess = VK_TRUE;
	VkPhysicalDeviceVulkan11Features f11 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES, &f12 };
	f11.storageBuffer16BitAccess = VK_TRUE;
	f11.shaderDrawParameters = VK_TRUE;
	VkPhysicalDeviceFeatures2 f2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &f11 };
	f2.features.shaderInt16 = VK_TRUE;
	f2.features.shaderInt64 = VK_TRUE;
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &f2 };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	VkDevice dev;
	CHECK(vkCreateDevice(pd, &dci, NULL, &dev));

	const VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	VkDescriptorSetLayoutBinding b0 = { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, stages, NULL };
	VkDescriptorSetLayoutBinding b1 = { 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, stages, NULL };
	VkDescriptorSetLayoutCreateInfo dslci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	VkDescriptorSetLayout dsl[2];
	dslci.bindingCount = 1;
	dslci.pBindings = &b0;
	CHECK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl[0]));
	dslci.pBindings = &b1;
	CHECK(vkCreateDescriptorSetLayout(dev, &dslci, NULL, &dsl[1]));
	VkPipelineLayoutCreateInfo plci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
	plci.setLayoutCount = 2;
	plci.pSetLayouts = dsl;
	VkPipelineLayout layout;
	CHECK(vkCreatePipelineLayout(dev, &plci, NULL, &layout));

	VkShaderModuleCreateInfo smci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
	VkShaderModule vs, fs;
	smci.codeSize = vs_size;
	smci.pCode = vs_code;
	CHECK(vkCreateShaderModule(dev, &smci, NULL, &vs));
	smci.codeSize = fs_size;
	smci.pCode = fs_code;
	CHECK(vkCreateShaderModule(dev, &smci, NULL, &fs));

	VkPipelineShaderStageCreateInfo st[2] = { { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
		{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO } };
	st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	st[0].module = vs;
	st[0].pName = "main";
	st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	st[1].module = fs;
	st[1].pName = "main";

	VkPipelineVertexInputStateCreateInfo vi = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
	VkPipelineInputAssemblyStateCreateInfo ia = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkRect2D scissor = { { 0, 0 }, { 0x7fffffff, 0x7fffffff } };
	VkPipelineViewportStateCreateInfo vp = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	vp.viewportCount = 1;
	vp.scissorCount = 1;
	vp.pScissors = &scissor;
	VkPipelineRasterizationStateCreateInfo rs = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	rs.cullMode = VK_CULL_MODE_BACK_BIT;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	ds.depthTestEnable = VK_TRUE;
	ds.depthWriteEnable = VK_TRUE;
	ds.depthCompareOp = VK_COMPARE_OP_GREATER;
	VkPipelineColorBlendAttachmentState att = { 0 };
	att.colorWriteMask = 0xF;
	VkPipelineColorBlendStateCreateInfo cb = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	cb.attachmentCount = 1;
	cb.pAttachments = &att;
	VkDynamicState dyn = VK_DYNAMIC_STATE_VIEWPORT;
	VkPipelineDynamicStateCreateInfo dy = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dy.dynamicStateCount = 1;
	dy.pDynamicStates = &dyn;
	VkFormat color_fmt = VK_FORMAT_B8G8R8A8_SRGB;
	VkPipelineRenderingCreateInfo ri = { VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	ri.colorAttachmentCount = 1;
	ri.pColorAttachmentFormats = &color_fmt;
	ri.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;

	VkGraphicsPipelineCreateInfo gpci = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO, &ri };
	gpci.stageCount = 2;
	gpci.pStages = st;
	gpci.pVertexInputState = &vi;
	gpci.pInputAssemblyState = &ia;
	gpci.pViewportState = &vp;
	gpci.pRasterizationState = &rs;
	gpci.pMultisampleState = &ms;
	gpci.pDepthStencilState = &ds;
	gpci.pColorBlendState = &cb;
	gpci.pDynamicState = &dy;
	gpci.layout = layout;
	gpci.basePipelineIndex = -1;

	printf("vkCreateGraphicsPipelines...\n");
	fflush(stdout);
	VkPipeline pipe;
	CHECK(vkCreateGraphicsPipelines(dev, VK_NULL_HANDLE, 1, &gpci, NULL, &pipe));
	printf("OK\n");

	vkDestroyPipeline(dev, pipe, NULL);
	vkDestroyShaderModule(dev, vs, NULL);
	vkDestroyShaderModule(dev, fs, NULL);
	vkDestroyPipelineLayout(dev, layout, NULL);
	vkDestroyDescriptorSetLayout(dev, dsl[0], NULL);
	vkDestroyDescriptorSetLayout(dev, dsl[1], NULL);
	vkDestroyDevice(dev, NULL);
	vkDestroyInstance(inst, NULL);
	return 0;
}
