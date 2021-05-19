mlx90640_video_i2c_postprocessing_sources = [
    'main.cpp',
    'mlx90640.cpp',
    'dev_handler.cpp'
]

mlx90640_video_i2c_postprocessing_deps = [
    dependency('gstreamer-1.0'),
    dependency('gstreamer-audio-1.0')
]

executable('mlx90640_video-i2c_postprocessing', mlx90640_video_i2c_postprocessing_sources,
    dependencies: mlx90640_video_i2c_postprocessing_deps,
    include_directories : include_directories('../include'),
    install: true,
)

push_data_sources = [
    'push_data.cpp'
]

executable('push_data', push_data_sources,
    dependencies: mlx90640_video_i2c_postprocessing_deps,
    include_directories : include_directories('../include'),
    install: true,
)
