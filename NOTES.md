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