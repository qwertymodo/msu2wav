#include <iostream>
#include <fstream>
#include <cstdint>
#include <string>

#include "getopt.h"

typedef struct _RIFF_HDR {
    struct CHUNK_DESC {
        char chunk_id[4];
        uint32_t chunk_size;
        char format[4];
    } chunk_descriptor;
	struct SUBCHUNK_1 {
		char chunk_id[4];
		uint32_t chunk_size;
		uint16_t audio_format;
		uint16_t num_channels;
		uint32_t sample_rate;
		uint32_t byte_rate;
		uint16_t block_align;
		uint16_t sample_size;
	} subchunk_1;
    struct SUBCHUNK_2 {
        char chunk_id[4];
        uint32_t chunk_size;
    } subchunk_2;
} RIFF_HDR;

typedef struct _SMPL_CHUNK {
    char chunk_id[4];
    uint32_t chunk_size;
    uint32_t manufacturer;
    uint32_t product;
    uint32_t period;
    uint32_t note;
    uint32_t pitch;
    uint32_t smpte_format;
    uint32_t smpte_offset;
    uint32_t count;
    uint32_t data;
    struct LOOP{
        uint32_t cue_id;
        uint32_t type;
        uint32_t start;
        uint32_t end;
        uint32_t fraction;
        uint32_t count;
    } loop;
} SMPL_CHUNK;

int main(int argc, char **argv)
{
    bool output_set = false, output_fname = false, output_smpl = false, output_stdout = false;
    int c;
    while ((c = getopt(argc, argv, "nos")) != -1)
    {
        switch (c)
        {
        case 'n':   // File name
            output_fname = true;
            output_set = true;
            break;

        case 'o':   // Stdout
            output_stdout = true;
            output_set = true;
            break;

        case 's':   // SMPL chunk
            output_smpl = true;
            output_set = true;
            break;

        case '?':   // Help
            fprintf(stderr, "Usage: msu2wav [options] [pcmfiles]\n\n  -n: Append loop point to output filename\n  -o: Print loop point to stdout\n  -s: Export loop point to smpl chunk");
            abort();

        default:
            fprintf(stderr, "Unknown option -%c.\n", optopt);
            abort();
        }
    }

    if (!output_set)
        output_fname = true;

	RIFF_HDR header;
    SMPL_CHUNK smpl_chunk;

	header.chunk_descriptor.chunk_id[0] = 'R';
	header.chunk_descriptor.chunk_id[1] = 'I';
	header.chunk_descriptor.chunk_id[2] = 'F';
	header.chunk_descriptor.chunk_id[3] = 'F';
	header.chunk_descriptor.chunk_size = sizeof(header) - 8;
	header.chunk_descriptor.format[0] = 'W';
	header.chunk_descriptor.format[1] = 'A';
	header.chunk_descriptor.format[2] = 'V';
	header.chunk_descriptor.format[3] = 'E';
	header.subchunk_1.chunk_id[0] = 'f';
	header.subchunk_1.chunk_id[1] = 'm';
	header.subchunk_1.chunk_id[2] = 't';
	header.subchunk_1.chunk_id[3] = ' ';
	header.subchunk_1.chunk_size = sizeof(header.subchunk_1) - 8;
	header.subchunk_1.audio_format = 1; // Uncompressed PCM
	header.subchunk_1.num_channels = 2;
	header.subchunk_1.sample_rate = 44100;
	header.subchunk_1.sample_size = 16;
	header.subchunk_1.byte_rate = header.subchunk_1.num_channels \
										* header.subchunk_1.sample_size  \
										* header.subchunk_1.sample_rate  \
										/ 8;
	header.subchunk_1.block_align = header.subchunk_1.num_channels \
										  * header.subchunk_1.sample_size  \
										  / 8;
	header.subchunk_2.chunk_id[0] = 'd';
	header.subchunk_2.chunk_id[1] = 'a';
	header.subchunk_2.chunk_id[2] = 't';
	header.subchunk_2.chunk_id[3] = 'a';

    if (output_smpl)
    {
        memset(&smpl_chunk, 0, sizeof(smpl_chunk));
        smpl_chunk.chunk_id[0] = 's';
        smpl_chunk.chunk_id[1] = 'm';
        smpl_chunk.chunk_id[2] = 'p';
        smpl_chunk.chunk_id[3] = 'l';
        smpl_chunk.chunk_size = sizeof(smpl_chunk) - 8;
        smpl_chunk.note = 60;
        smpl_chunk.count = 1;
    }

	for (auto i = optind; i < argc; ++i)
	{
		std::string fname(argv[i]);
        std::string ofname;
		std::ifstream msu_file;
		std::ofstream wav_file;

		msu_file.open(fname, std::ios::binary | std::ios::ate);
		if (msu_file.is_open())
		{
			header.subchunk_2.chunk_size = (uint32_t)msu_file.tellg() - 8;
            header.chunk_descriptor.chunk_size += header.subchunk_2.chunk_size;
            if (output_smpl)
            {
                header.chunk_descriptor.chunk_size += (sizeof(smpl_chunk));
                smpl_chunk.loop.end = ((uint32_t)msu_file.tellg() - 12) / 4;
            }

			msu_file.seekg(0);

			if (msu_file.get() != 'M')
				break;
			if (msu_file.get() != 'S')
				break;
			if (msu_file.get() != 'U')
				break;
			if (msu_file.get() != '1')
				break;

			uint32_t loop;
			msu_file.read((char *)&loop, sizeof(loop));
            if (output_smpl)
                smpl_chunk.loop.start = loop;

            if (output_stdout)
                std::cout << loop << std::endl;

            ofname = fname.erase(fname.find_last_of("."), std::string::npos);
            if (output_fname)
                ofname.append("__lp" + std::to_string(loop));

			wav_file.open(ofname + ".wav", std::ios::binary | std::ios::trunc);
			if (!wav_file.is_open())
				break;
			
			wav_file.write((char *)&header, sizeof(header));
			wav_file << msu_file.rdbuf();

            if (output_smpl)
                wav_file.write((char *)&smpl_chunk, sizeof(smpl_chunk));

            wav_file.close();
		}
	}

	return 0;
}