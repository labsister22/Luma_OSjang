#include "stdint.h"

struct SegmentDescriptor
{
  // First 32-bit
  uint16_t segment_low;
  uint16_t base_low;

  // Next 16-bit (Bit 32 to 47)
  uint8_t base_mid;
  uint8_t type_bit : 4;
  uint8_t non_system : 1;
  // TODO : Continue SegmentDescriptor definition

} __attribute__((packed));
