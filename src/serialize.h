/* am7xxx - communication with AM7xxx based USB Pico Projectors and DPFs
 *
 * Copyright (C) 2012  Antonio Ospite <ospite@studenti.unina.it>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/* You can transform a serialization block of code which uses put-* into the
 * correspondent unserialization block with this vim substitution pattern:
 *
 *   s/put_\([^(]*\)(\([^,]*\),\s*\([^)]*\))/\2 = get_\1(\3)/g
 */

#ifndef __SERIALIZE_H
#define __SERIALIZE_H

#include <stdint.h>

uint8_t get_8(uint8_t **bufferp);
uint32_t get_le32(uint8_t **bufferp);

uint8_t *put_8(uint8_t value, uint8_t **bufferp);
uint8_t *put_le32(uint32_t value, uint8_t **bufferp);

#endif /* __SERIALIZE_H */
