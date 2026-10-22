import math

class SkyMatrix:

    TREGENZA_PATCH_COUNT = 145
    TREGENZA_PATCHES_PER_ROW = [30, 30, 24, 24, 18, 12, 6, 1]
    NUM_ROWS = len(TREGENZA_PATCHES_PER_ROW)
    DEGREES_PER_ROW = 12.0
    DEG2RAD = math.pi / 180.0

    @staticmethod
    def date_to_hoy(_date):
        from ladybug.dt import DateTime

        if not isinstance(_date, DateTime):
            _date = DateTime(_date.month, _date.day, _date.hour)

        return int(_date.hoy)

    @staticmethod
    def compute(_epw_path, _start_hoy, _end_hoy, _only_sun=False):

        from ladybug_radiance.config import folders

        folders.radiance_path = (
            "C:/Users/cfreude/data/projects/tamashii/external_bin/radiance"
        )
        from ladybug_radiance import skymatrix
        import numpy as np

        print(f"start_hoy: {_start_hoy}, end_hoy: {_end_hoy}")

        hoys = range(_start_hoy, _end_hoy)

        from ladybug_radiance.skymatrix import EPW, Wea

        wea = Wea.from_epw_file(_epw_path)
        wea = wea.filter_by_hoys(hoys)
        skymtx = skymatrix.SkyMatrix(wea)
        skymtx.folder = "./"

        wea_duration = len(skymtx.wea) / skymtx.wea.timestep

        print("wea_duration: ", wea_duration)
        skymtx.compute_sky()

        if _only_sun:
            values = skymtx.direct_values
        else:
            values = np.add(skymtx.direct_values, skymtx.diffuse_values)

        return values


# TEST FUNCTION
if __name__ == "__main__":

    # start_datetime = DateTime(6, 15, 12, 0)
    # end_datetime = DateTime(7, 15, 12, 0)

    import datetime
    from dateutil import parser

    start_datetime = parser.parse("2022-06-01T6:00:00+00:00")
    start_datetime.replace(tzinfo=datetime.timezone.utc)

    end_datetime = parser.parse("2022-06-01T7:00:00+00:00")
    end_datetime.replace(tzinfo=datetime.timezone.utc)

    start_hoy = SkyMatrix.date_to_hoy(start_datetime)
    end_hoy = SkyMatrix.date_to_hoy(end_datetime)

    epw_path = "C:/Users/cfreude/data/projects/tamashii/tools/blender-thermal-plugin/AUT_Vienna.Schwechat.110360_IWEC.epw"

    sky = SkyMatrix()
    values = sky.compute(epw_path, start_hoy, end_hoy, _only_sun=False)

    print(values)
