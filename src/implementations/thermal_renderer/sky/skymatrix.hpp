#include <string>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <iterator>

#include <stdio.h>
#include <stdlib.h>

#include "date/date.h"

using namespace std;
namespace chr = chrono;
namespace fs = filesystem;

class SkyMatrix
{

public:

    SkyMatrix(const string radiance_bin_path_str = "./external_bin/radiance/bin") : radiance_bin_path(radiance_bin_path_str), sky_values{} {}
    ~SkyMatrix() {}

    void set_radiance_bin_path(const string radiance_bin_path_str) { radiance_bin_path = radiance_bin_path_str; }

    const float* get_sky_values() const { return sky_values; }

    void set_epw(const string epw_path_str)
    {
        fs::path epw_path(epw_path_str);
        string cmd1 = (radiance_bin_path / "epw2wea").lexically_normal().string() + " " + epw_path.lexically_normal().string() + " original.wea";
        spdlog::info("Calling: {}", cmd1);
        std::cout.flush();
        system(cmd1.c_str());
    }

    const float* get_data(const int begin_hoy, const int end_hoy)
    {
        if (filter_wea_file("original.wea", "filtered.wea", begin_hoy, end_hoy))
            std::cerr << "Error in filter_wea_file()" << endl;

        // TODO: possibly compute all (without -A) and keep in memory and then filter on the fly
        string cmd2 = (radiance_bin_path / "gendaymtx").lexically_normal().string() + " -m 1 -O1 -A filtered.wea > sky.mtx";
        spdlog::info("Calling: {}", cmd2);
        std::cout.flush();
        system(cmd2.c_str());

        int hours = end_hoy - begin_hoy;

        spdlog::info("WEA duration: {} h.", hours);

        get_sky_values("sky.mtx", hours, sky_values, NUM_SKY_VALUES);

        return sky_values;
    }

    const float* get_data(const string begin_datetime, const string end_datatime = string())
    {
        int begin_hoy = datetime_to_hoy(begin_datetime);
        int end_hoy = begin_hoy;

        if (!end_datatime.empty())
            end_hoy = datetime_to_hoy(end_datatime);

        cout << "Computing sky from " << begin_datetime << " to " << end_datatime << " (" << end_hoy << "-" << begin_hoy << "=" << end_hoy - begin_hoy << " hoys)" << endl;

        return get_data(begin_hoy, end_hoy);
    }

    static int datetime_to_hoy(const string& datetime)
    {
        int year, month, day, hour, minute, second, dhour, dminute;
        // example "2022-06-01T012:00:00+00:00"
        sscanf(datetime.c_str(), "%d-%d-%dT%d:%d:%d+%d:%d", &year, &month, &day, &hour, &minute, &second, &dhour, &dminute);

        //cout << year << "." << month << "." << day << "-" << hour << ":" << minute << ":" << second << "+" << dhour << ":" << dminute << endl;

        // Convert the components into a time_point object
        date::sys_days date = date::year{ year } / month / day;
        chr::seconds time = chr::hours{ hour } + chr::minutes{ minute } + chr::seconds{ second };
        chr::time_point<chr::system_clock> tp = date + time;

        // Subtract the time_point object representing the start of the year
        date::sys_days start_of_year = date::year{ year } / 1 / 1;
        chr::time_point<chr::system_clock> tp0 = start_of_year;

        // Get the duration between the two time_points
        chr::duration<double> dur = tp - tp0;

        // Divide the duration by one hour to get the hour of the year
        double hoy = dur / chr::hours(1);

        return hoy;
    }

    static float convert_to_broadband_radiation(string patch_row_str, int row_number, float wea_duration, int sky_density = 1)
    {
        if (sky_density != 1)
            cerr << "Error only sky_density==1 supported (for now). Using 1." << endl;

        float R, G, B;
        stringstream ss(patch_row_str);
        ss >> R >> G >> B;
        float w_val = 0.265074126 * R + 0.670114631 * G + 0.064811243 * B;
        float coeff = TREGENZA_COEFFICIENTS[row_number];
        return w_val * 0.01; // HACK to align values with MTS // *coeff* wea_duration;
    }

    // from https://github.com/ladybug-tools/ladybug-radiance/blob/master/ladybug_radiance/skymatrix.py#L597
    static const int NUM_SKY_VALUES = 145;
    static const int NUM_ROWS = 8;
    static constexpr float DEGREES_PER_ROW = 12.0f;
    static constexpr int TREGENZA_PATCHES_PER_ROW[] = { 30, 30, 24, 24, 18, 12, 6, 1 };
    static constexpr float TREGENZA_COEFFICIENTS[] = { 0.0435449227, 0.0416418006, 0.0473984151, 0.0406730411, 0.0428934136, 0.0445221864, 0.0455168385, 0.0344199465 };

private:

    fs::path radiance_bin_path;
    float sky_values[NUM_SKY_VALUES];

    int filter_wea_file(string wea_input_file, string wea_output_file, int begin_hoy, int end_hoy)
    {
        // Open the input file
        ifstream fin(wea_input_file);
        if (!fin)
        {
            cerr << "Error opening wea_input_file" << endl;
            return 1;
        }

        // Open the temporary output file
        ofstream fout(wea_output_file);
        if (!fout)
        {
            cerr << "Error opening wea_output_file" << endl;
            return 2;
        }

        //cout << "begin_hoy: " << begin_hoy <<  "| end_hoy: " << end_hoy << endl;

        // Read the input file line by line
        string line;
        int count = 0;
        int num_extracted = 0;
        while (getline(fin, line))
        {
            if ((count < 6) || (((count-6) >= begin_hoy) && ((count-6) < end_hoy)))
            {
                fout << line << endl;
                num_extracted++;
            }
            count += 1;
        }

        cout << "WEA lines extracted: " << num_extracted << endl;

        // Close the files
        fin.close();
        fout.close();

        return 0;
    }

    void get_sky_values(const string mtx_path_str, int wea_duration_in_h, float* const values, int size)
    {
        // Open the input file
        ifstream fin(mtx_path_str);
        if (fin)
        {
            string line;

            // skip header and ground patch
            for (int i = 0; i < 9; i++)
                getline(fin, line);

            int count = 0;
            for (int row_number = 0; row_number < NUM_ROWS; row_number++)
            {
                for (int i = 0; i < TREGENZA_PATCHES_PER_ROW[row_number]; i++)
                {
                    if (getline(fin, line))
                    {
                        values[count++] = convert_to_broadband_radiation(line, row_number, wea_duration_in_h, 1);
                    }
                }
            }
        }
        else
            cerr << "Error opening mtx_path_str" << endl;
    }
};


class SkyTexture
{

public:
    SkyTexture(const int w = 240, const int h = 128) : tex_w(w), tex_h(h)
    {
        texture_data = new float[tex_w * tex_h * 4];
    }

    ~SkyTexture()
    {
        delete[] texture_data;
    }

    int get_data_width() { return tex_w; }
    int get_data_height() { return tex_h; }

private:

    int tex_w, tex_h = 0;
    float* texture_data = 0;
    float tex_max = 0;

    void save_ppm(string path)
    {
        if (texture_data == 0) {
            wcout << "Can't save PPM. Texture not initialized." << endl;
            return;
        }

        cout << "Saving texture to: " << path << endl;
        std::ofstream file(path);

        //  header
        file << "P2\n" << tex_w << " " << tex_h << "\n255\n";

        if (tex_max == 0)
        {
            wcout << "Max texture value is 0." << endl;
            tex_max = 1.0;
        }

        int offset = 0;
        for (int i = 0; i < tex_h; i++) {
            for (int j = 0; j < tex_w; j++)
            {
                file << static_cast<int>((texture_data[offset] / tex_max) * 255) << " ";
                //    << static_cast<int>((texture_data[offset++] / tex_max) * 255) << " "
                //    << static_cast<int>((texture_data[offset++] / tex_max) * 255);

                offset += 4;
            }
            file << "\n";
        }

        // Close the file stream
        file.close();
    }

    void initialize_texture(const SkyMatrix& _skyMatrix, bool _flip = false)
    {
        const float* sky_values = _skyMatrix.get_sky_values();
        
        tex_max = 0;

        int offset = 0; // texture offset

        int ystep_per_row = round(float(tex_h / 2) / (SkyMatrix::NUM_ROWS - 0.5));

        int num_processed_patches = 0;
        int tex_offset = 0;

        if (_flip)
            tex_offset = tex_w * tex_h * 4 - 1;

        for (int i = 0; i < tex_w * tex_h * 4; i++)
            texture_data[i] = 0.0;

        //cout << "height/2: " << h/2 << " | ystep_per_row: " << ystep_per_row << endl;

        for (int patch_row = SkyMatrix::NUM_ROWS - 1; patch_row >= 0; patch_row--)
        {
            // upsample patches to texture width

            int patches_per_row = SkyMatrix::TREGENZA_PATCHES_PER_ROW[patch_row];
            float* upsampled_row = new float[tex_w]();

            int patch_index_start = SkyMatrix::NUM_SKY_VALUES - 1 - num_processed_patches;

            int step = round(float(tex_w) / patches_per_row);
            //cout << "row: " << patch_row << " | ppr: " << patches_per_row << " | step: " << step << " | ";
            //cout << patch_row << " " << patches_per_row << " " << step << endl;

            int diff = tex_w - step * patches_per_row;
            int pad = diff / 2;

            int offset = 0;
            int last_patch_row_index = 0;

            if (pad > 0)
            {
                float patch_value = sky_values[patch_index_start];
                for (int i = 0; i < pad; i++)
                {
                    if (offset < tex_w) {
                        upsampled_row[offset++] = patch_value;
                    }
                    else {
                        break;
                    }
                }
            }

            for (int row_patch_index = 0; row_patch_index < patches_per_row; row_patch_index++)
            {
                int patch_index = patch_index_start - row_patch_index;
                float patch_value = sky_values[patch_index];
                tex_max = std::max(tex_max, patch_value);

                for (int i = 0; i < step; i++)
                {
                    if (offset < tex_w) {
                        upsampled_row[offset++] = patch_value;
                        last_patch_row_index = row_patch_index;
                        //cout << std::setw(5) << std::setprecision(4) << patch_value << ", "; 
                    }
                    else {
                        break;
                    }
                }
            }
            //cout << endl;

            if ((diff - pad) > 0)
            {
                int patch_index = patch_index_start - last_patch_row_index;
                float patch_value = sky_values[patch_index];
                for (int i = 0; i < (diff - pad); i++)
                {
                    if (offset < tex_w) {
                        upsampled_row[offset++] = patch_value;
                    }
                    else {
                        break;
                    }
                }
            }

            if (last_patch_row_index + 1 < patches_per_row || offset < tex_w)
            {
                //cout << "| lpi: " << last_patch_row_index+1 << "/" << patches_per_row << " | offset: " << offset;
                wcout << "Suboptimal texture width. Possible loss of patches." << endl;
            }

            num_processed_patches += patches_per_row;

            // copy row X times to texture

            int ystep = ystep_per_row;
            if (patch_row == SkyMatrix::NUM_ROWS - 1)
                ystep = int(ystep_per_row / 2);

            for (int i = 0; i < ystep; i++)
            {
                if (_flip)
                {
                    for (int x = tex_w - 1; x >= 0; x--)
                    {
                        float val = upsampled_row[x];
                        texture_data[tex_offset--] = 1.0;
                        texture_data[tex_offset--] = val;
                        texture_data[tex_offset--] = val;
                        texture_data[tex_offset--] = val;
                        //cout << "[ " << val << ", " << val << ", " << val << " ]" << endl;
                    }
                }
                else
                {
                    for (int x = tex_w - 1; x >= 0; x--)
                    {
                        float val = upsampled_row[x];
                        texture_data[tex_offset++] = val;
                        texture_data[tex_offset++] = val;
                        texture_data[tex_offset++] = val;
                        texture_data[tex_offset++] = 1.0;
                        //cout << "[ " << val << ", " << val << ", " << val << " ]" << endl;
                    }
                }
            }

            delete[] upsampled_row;
        }


        std::ofstream envcsv;
        envcsv.open("env.csv");
        size_t c = 0;
        spdlog::info("tex_w: {}, tex_h: {}", tex_w, tex_h);
        for (int h = 0; h < tex_h; h++)
        {
            for (int w = 0; w < tex_w; w++)
            {
                envcsv << texture_data[c] << ", ";
                //envcsv << texture_data[c+1] << ", ";
                //envcsv << texture_data[c+2] << ", ";
                //envcsv << texture_data[c+3] << ", "; // alpha
                c += 4;
            }
            envcsv << "\n";
        }
        envcsv.close();

        save_ppm("./env.ppm");
    }
};