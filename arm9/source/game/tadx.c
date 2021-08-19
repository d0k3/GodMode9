#include "tadx.h"
#include "cia.h"

u32 ValidateTadXHeader(TadXHeader* header) {
	// super strict validation
    if ((getbe32(header->size_header) != TADX_HEADER_SIZE) ||
        (getbe32(header->size_cert) != TADX_CERT_SIZE) ||
        (getbe32(header->size_ticket) != TICKET_TWL_SIZE) ||
        (getbe32(header->size_tmd) != TMD_SIZE_TWL) ||
        (getbe32(header->reserved0) != 0) ||
        (getbe32(header->reserved1) != 0) ||
        (getbe32(header->size_content) == 0))
        return 1;
    return 0;
}
