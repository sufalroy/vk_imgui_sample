# Vulkan Memory Notes

## Single Allocation
- Allocate one large `VkDeviceMemory`
- Sub-allocate for vertex, index, uniform buffers

## Single Buffer Strategy
- Store vertex + index data in one `VkBuffer`
- Use offsets when binding

## Benefits
- Better cache locality
- Fewer bindings
- Lower allocation overhead

## Aliasing
- Reuse memory for different resources
- Only if not used simultaneously
- Must refresh data before reuse

## Notes
- Requires proper synchronization
- Useful for staging and per-frame resources


 For practical applications it is recommended to combine these operations in a single command buffer and execute them asynchronously for higher throughput, especially the transitions and copy in the createTextureImage function. Try to experiment with this by creating a setupCommandBuffer that the helper functions record commands into, and add a flushSetupCommands to execute the commands that have been recorded so far. It’s best to do this after the texture mapping works to check if the texture resources are still set up correctly.