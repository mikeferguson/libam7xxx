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

#include <string.h>

#ifdef __MINGW32__
#define le32toh(x) (x)
#define htole32(x) (x)
#else
#include <endian.h>
#endif

#include "serialize.h"

uint8_t get_8(uint8_t **bufferp)
{
	uint8_t tmp;

	tmp = *bufferp[0];
	*bufferp += 1;

	return tmp;
}

uint32_t get_le32(uint8_t **bufferp)
{
	uint32_t tmp;

	memcpy(&tmp, *bufferp, sizeof (tmp));
	*bufferp += sizeof (tmp);

	return le32toh(tmp);
}

uint8_t *put_8(uint8_t value, uint8_t **bufferp)
{
	*bufferp[0] = value;
	*bufferp += 1;

	return *bufferp;
}

uint8_t *put_le32(uint32_t value, uint8_t **bufferp)
{
	uint32_t tmp;

	tmp = htole32(value);
	memcpy(*bufferp, &tmp, sizeof (tmp));
	*bufferp += sizeof (tmp);

	return *bufferp;
}
