/* picoproj - communication with AM7xxx based USB pico projectors
 *
 * Copyright (C) 2011  Antonio Ospite <ospite@studenti.unina.it>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <endian.h>
#include <errno.h>

/*
 * Examples of a packet headers:
 *
 * Image widget:
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * +02|00|00|00|00|10|3e|10|01|00|00|00|20|03|00|00|e0|01|00|00|53|E8|00|00+
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 * Brightness widget:
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * +04|00|00|00|00|0c|ff|ff|00|00|00|00|00|00|00|00|00|00|00|00|00|00|00|00+
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 */

#define AM7x01_PACKET_TYPE_INIT	   0x01
#define AM7x01_PACKET_TYPE_IMAGE   0x02
#define AM7x01_PACKET_TYPE_POWER   0x04
#define AM7x01_PACKET_TYPE_UNKNOWN 0x05

struct buffer {
	unsigned int len;
	uint8_t *data;
};

struct header{
	uint32_t packet_type;
	uint32_t unknown1;
	uint32_t unknown2;
	uint32_t width;
	uint32_t height;
	uint32_t payload_size;
};

struct packet {
	struct header header;
	uint8_t payload[];
};

static struct buffer *packet_allocate_buffer(struct packet *p)
{
	struct buffer *buffer;

	if (p == NULL) {
		perror("packet NULL");
		return NULL;
	}

	buffer = malloc(sizeof(*buffer));
	if (buffer == NULL) {
		perror("malloc buffer");
		return NULL;
	}

	buffer->len = sizeof(p->header); /* + p->header.payload_size; */

	buffer->data = malloc(buffer->len);
	if (buffer->data == NULL) {
		perror("malloc buffer->data");
		free(buffer);
		return NULL;
	}
	return buffer;
}

static void packet_free_buffer(struct buffer *buffer)
{
	free(buffer->data);
	free(buffer);
	buffer = NULL;
}

static int packet_pack(struct packet *p, struct buffer *buffer)
{
	unsigned int offset;
	uint32_t tmp;

	if (p == NULL || buffer == NULL)
		return -EINVAL;

	/* TODO: check for packet payload being NULL? */
	if (buffer->data == NULL || buffer->len < sizeof(*p))
		return -EINVAL;

	offset = 0;

	tmp = htole32(p->header.packet_type);
	memcpy(buffer->data + offset, &tmp, sizeof(p->header.packet_type));
	offset += sizeof(p->header.packet_type);

	tmp = htole32(p->header.unknown1);
	memcpy(buffer->data + offset, &tmp, sizeof(p->header.unknown1));
	offset += sizeof(p->header.unknown1);

	tmp = htole32(p->header.unknown2);
	memcpy(buffer->data + offset, &tmp, sizeof(p->header.unknown2));
	offset += sizeof(p->header.unknown2);

	tmp = htole32(p->header.width);
	memcpy(buffer->data + offset, &tmp, sizeof(p->header.width));
	offset += sizeof(p->header.width);

	tmp = htole32(p->header.height);
	memcpy(buffer->data + offset, &tmp, sizeof(p->header.height));
	offset += sizeof(p->header.height);

	tmp = htole32(p->header.payload_size);
	memcpy(buffer->data + offset, &tmp, sizeof(p->header.payload_size));
	offset += sizeof(p->header.payload_size);

	/* TODO memcpy payload of size */

	return 0;
}

static int packet_unpack(struct buffer *buffer, struct packet *p)
{
	unsigned int offset;
	uint32_t tmp;

	if (p == NULL || buffer == NULL)
		return -EINVAL;

	/* TODO: check for packet payload being NULL? */
	if (buffer->data == NULL || buffer->len < sizeof(*p))
		return -EINVAL;

	offset = 0;

	memcpy(&tmp, buffer->data + offset, sizeof(p->header.packet_type));
	p->header.packet_type = le32toh(tmp);
	offset += sizeof(p->header.packet_type);

	memcpy(&tmp, buffer->data + offset, sizeof(p->header.unknown1));
	p->header.unknown1 = le32toh(tmp);
	offset += sizeof(p->header.unknown1);

	memcpy(&tmp, buffer->data + offset, sizeof(p->header.unknown2));
	p->header.unknown2 = le32toh(tmp);
	offset += sizeof(p->header.unknown2);

	memcpy(&tmp, buffer->data + offset, sizeof(p->header.width));
	p->header.width = le32toh(tmp);
	offset += sizeof(p->header.width);

	memcpy(&tmp, buffer->data + offset, sizeof(p->header.height));
	p->header.height = le32toh(tmp);
	offset += sizeof(p->header.height);

	memcpy(&tmp, buffer->data + offset, sizeof(p->header.payload_size));
	p->header.payload_size = le32toh(tmp);
	offset += sizeof(p->header.payload_size);

	/* malloc & memcpy payload of size p->header.payload_size */

	return 0;
}

static void packet_dump_header(struct packet *p)
{
	if (p == NULL)
		return;

	printf("packet_type: 0x%08x (%u)\n", p->header.packet_type, p->header.packet_type);
	printf("unknown1:    0x%08x (%u)\n", p->header.unknown1, p->header.unknown1);
	printf("unknown2:    0x%08x (%u)\n", p->header.unknown2, p->header.unknown2);
	printf("width:       0x%08x (%u)\n", p->header.width, p->header.width);
	printf("height:      0x%08x (%u)\n", p->header.height, p->header.height);
	printf("size:        0x%08x (%u)\n", p->header.payload_size, p->header.payload_size);
	fflush(stdout);
}

static void packet_dump_buffer(struct buffer *buffer)
{
	unsigned int i;

	if (buffer == NULL)
		return;

	if (buffer->data == NULL)
		return;

	for (i = 0; i < buffer->len; i++) {
		printf("%02hhX%c", buffer->data[i], (i < buffer->len - 1) ? ' ' : '\n');
	}
	fflush(stdout);
}

int main(void)
{
	struct packet p1  = {
		.header = {
			.packet_type  = AM7x01_PACKET_TYPE_IMAGE,
			.unknown1     = le32toh(0x103e1000),
			.unknown2     = le32toh(0x00000001),
			.width        = 800,
			.height       = 480,
			.payload_size = 59475,
		},
		/* TODO initialize payload */
	};
	struct buffer *buffer = NULL;
	struct packet p2;
	int ret;

	packet_dump_header(&p1);

	buffer = packet_allocate_buffer(&p1);
	if (buffer == NULL) {
		fprintf(stderr, "Cannot allocate the buffer.\n");
		exit(EXIT_FAILURE);
	}

	ret = packet_pack(&p1, buffer);
	if (ret < 0) {
		fprintf(stderr, "Cannot pack the packet.\n");
		exit(EXIT_FAILURE);
	}

	packet_dump_buffer(buffer);

	ret = packet_unpack(buffer, &p2);
	if (ret < 0) {
		fprintf(stderr, "Cannot unpack the packet.\n");
		exit(EXIT_FAILURE);
	}

	packet_dump_header(&p2);

	packet_free_buffer(buffer);

	exit(EXIT_SUCCESS);
}
