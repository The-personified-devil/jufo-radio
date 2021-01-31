#include <iostream>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <filesystem>
#include <thread>

#include <SDL2/SDL.h>
#include <vlcpp/vlc.hpp>

#include "../external/lvgl/lvgl/lvgl.h"
#include "../external/lvgl/lv_drivers/display/monitor.h"
#include "../external/lvgl/lv_drivers/indev/mouse.h"


//#define _DEFAULT_SOURCE /* needed for usleep() */
#define SDL_MAIN_HANDLED /*To fix SDL's "undefined reference to WinMain" issue*/

static void hal_init(void);
static int tick_thread(void *data);
static void memory_monitor(lv_task_t *param);
void btn_event_handler(lv_obj_t * obj, lv_event_t event);
// void list_file_event_handler (lv_obj_t * obj, lv_event_t event);
// void list_dir_event_handler (lv_obj_t * obj, lv_event_t event);
// void list_sel_file_handler (lv_obj_t * obj, lv_event_t event);
void onEndReached ();
// void listdrag(lv_obj_t *obj, lv_event_t event);

lv_indev_t *kb_indev;
lv_indev_t *mouse_indev;

class mp
{
  protected:
    VLC::Instance vlcInstance = VLC::Instance(0, nullptr);

  public:
    VLC::MediaPlayer plr = VLC::MediaPlayer(vlcInstance);
    VLC::MediaPlayerEventManager evMgr  = plr.eventManager();
    VLC::MediaList mlist = VLC::MediaList();
    

  mp()
  {
    auto regevent = evMgr.onEndReached(onEndReached); 
  }

  void setMedia(std::string mediaFile)
  {
    auto media = VLC::Media(vlcInstance, mediaFile, VLC::Media::FromPath);
    plr.setMedia(media);
    std::cout << "Yeet" << std::endl;
    
    mlist.setMedia(media);
  }
} mp_obj;


class mp_gui : private mp
{
  public:
    lv_obj_t * label;

    void plb_btn(void)
    {
      static lv_style_t style;
      lv_style_init(&style);
      lv_style_set_text_font(&style, LV_STATE_DEFAULT, &lv_font_montserrat_48);
      
      lv_obj_t * btn2 = lv_btn_create(lv_scr_act(), NULL);
      lv_obj_set_event_cb(btn2, btn_event_handler);
      lv_obj_align(btn2, NULL, LV_ALIGN_CENTER, 0, 40);
      lv_btn_set_checkable(btn2, true);
      lv_btn_toggle(btn2);
      lv_btn_set_fit2(btn2, LV_FIT_NONE, LV_FIT_TIGHT);

      
      label = lv_label_create(btn2, NULL);
      lv_obj_add_style(label, LV_LABEL_PART_MAIN, &style);
      lv_label_set_text(label, LV_SYMBOL_PLAY);  
    }

    void setPlay()
    {
      lv_label_set_text(label, LV_SYMBOL_PLAY);
    }

    void create_ddl()
    {
      namespace fs = std::filesystem;

      std::string path = ".";

      /*Create a normal drop down list*/
      lv_obj_t * ddlist = lv_dropdown_create(lv_scr_act(), NULL);
      lv_dropdown_clear_options(ddlist);

      for (const auto & entry : fs::directory_iterator(path))
      {
        std::string file = entry.path().string();

        if (file.find(".m4a") != std::string::npos || file.find(".mp3") != std::string::npos || \
            file.find(".wma") != std::string::npos || file.find(".wav") != std::string::npos || \
            file.find(".ogg") != std::string::npos || file.find(".flac") != std::string::npos || \
            file.find(".aac") != std::string::npos)
        {
          // mp_obj.audio_Files.push_back(file); // Use VLC Playlist
          file.replace(file.find("./"), 2, LV_SYMBOL_AUDIO" ");
          lv_dropdown_add_option(ddlist, file.c_str(), LV_DROPDOWN_POS_LAST);
        }

        lv_obj_t * progress_bar = lv_slider_create(lv_scr_act(), NULL);
        lv_obj_align(progress_bar, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -40);
        lv_slider_set_value(progress_bar, 60, LV_ANIM_ON);

        // lv_obj_t * volume_bar = lv_slider_create(lv_scr_act(), NULL);
        // lv_obj_align(volume_bar, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, 25, -170);
        // lv_slider_set_value(volume_bar, 30, LV_ANIM_ON);
        // lv_obj_set_size(volume_bar, 10, 100);

        lv_obj_t * volume_btn = lv_label_create(lv_scr_act(), NULL);
        lv_obj_align(volume_btn, NULL, LV_ALIGN_IN_BOTTOM_MID, 175, -47);
        static lv_style_t stl;
        lv_style_init(&stl);
        lv_style_set_text_font(&stl, LV_STATE_DEFAULT, &lv_font_montserrat_32);
        lv_obj_add_style(volume_btn, LV_LABEL_PART_MAIN, &stl);
        lv_label_set_text(volume_btn, LV_SYMBOL_VOLUME_MID);

        lv_obj_t * home_btn = lv_label_create(lv_scr_act(), NULL);
        lv_obj_align(home_btn, NULL, LV_ALIGN_IN_TOP_LEFT, 30, 20);
        static lv_style_t home_style;
        lv_style_init(&home_style);
        lv_style_set_text_font(&home_style, LV_STATE_DEFAULT, &lv_font_montserrat_32);
        lv_obj_add_style(home_btn, LV_LABEL_PART_MAIN, &home_style);
        lv_label_set_text(home_btn, LV_SYMBOL_HOME);


        lv_obj_t * left_btn = lv_label_create(lv_scr_act(), NULL);
        lv_obj_align(left_btn, NULL, LV_ALIGN_IN_BOTTOM_MID, -180, -45);
        static lv_style_t left_style;
        lv_style_init(&left_style);
        lv_style_set_text_font(&left_style, LV_STATE_DEFAULT, &lv_font_montserrat_32);
        lv_obj_add_style(left_btn, LV_LABEL_PART_MAIN, &left_style);
        lv_label_set_text(left_btn, LV_SYMBOL_LEFT);


        lv_obj_t * right_btn = lv_label_create(lv_scr_act(), NULL);
        lv_obj_align(right_btn, NULL, LV_ALIGN_IN_BOTTOM_MID, -150, -45);
        static lv_style_t right_style;
        lv_style_init(&right_style);
        lv_style_set_text_font(&right_style, LV_STATE_DEFAULT, &lv_font_montserrat_32);
        lv_obj_add_style(right_btn, LV_LABEL_PART_MAIN, &right_style);
        lv_label_set_text(right_btn, LV_SYMBOL_RIGHT);
      }
      
      // lv_obj_set_event_cb(ddlist, dropdown_event_handler);
      lv_obj_set_width(ddlist, 400);
      lv_obj_align(ddlist, NULL, LV_ALIGN_IN_TOP_MID, 0, 20);
      lv_dropdown_open(ddlist);
    }
} mp_gui_obj;

// /*  TODO: Add behaviour for no selection and clear and make folders without direct audio files unselectable and make directories unrecursive */
// class filemgr
// {
//   public:

//     std::vector<std::string> files;
//     std::vector<std::string> dirs;
//     bool isdir;
//     lv_obj_t *current_file;
//     lv_obj_t *current_dir;

//     filemgr()
//     {

//     }

//     void filelist()
//     {
//       namespace fs = std::filesystem;
//       std::string path = ".";

//       /*Create a list*/
//       lv_obj_t * list1 = lv_list_create(lv_scr_act(), NULL);
//       lv_obj_set_size(list1, 800, 192);
//       lv_obj_align(list1, NULL, LV_ALIGN_CENTER, 0, 0);
//       // lv_obj_set_event_cb(list1, listdrag);

//       static lv_style_t style_list_btn;
//       lv_style_init(&style_list_btn);
//       lv_style_set_radius(&style_list_btn, LV_STATE_DEFAULT, 0);
//       lv_style_set_clip_corner(&style_list_btn, LV_STATE_DEFAULT, true);

//       static lv_style_t list_btn_style;
//       lv_style_init(&list_btn_style);
//       lv_style_set_outline_width(&list_btn_style, LV_STATE_FOCUSED, 0);
//       lv_style_set_border_side(&list_btn_style, LV_STATE_DEFAULT, LV_BORDER_SIDE_NONE);

//       lv_obj_t *play_file_btn = lv_btn_create(lv_scr_act(), NULL);
//       // lv_obj_set_event_cb(play_file_btn, list_sel_file_handler);
//       lv_obj_set_drag(play_file_btn, true);
//       lv_obj_set_drag_throw(play_file_btn, true);


//       for (const auto & entry : fs::recursive_directory_iterator(path))
//       {
//         std::cout << "Entry" << entry << std::endl;
//         std::string file = entry.path().string();

//         if (file.find(".m4a") != std::string::npos || file.find(".mp3") != std::string::npos || \
//             file.find(".wma") != std::string::npos || file.find(".wav") != std::string::npos || \
//             file.find(".ogg") != std::string::npos || file.find(".flac") != std::string::npos || \
//             file.find(".aac") != std::string::npos)
//         {
//           lv_obj_t *list_btn = lv_list_add_btn(list1, LV_SYMBOL_AUDIO, file.c_str());
//           // lv_obj_set_event_cb(list_btn,  list_file_event_handler);
//           files.push_back(file);
//           lv_obj_add_style(list_btn, LV_BTN_PART_MAIN, &list_btn_style);
//           // lv_page_glue_obj(list_btn, false);
//           // lv_obj_set_drag(list_btn, true);
//         }
//         else if(entry.is_directory()) {
//           lv_obj_t *list_btn = lv_list_add_btn(list1, LV_SYMBOL_DIRECTORY, file.c_str());
//           // lv_obj_set_event_cb(list_btn,  list_dir_event_handler);
//           dirs.push_back(file);
//           lv_obj_add_style(list_btn, LV_BTN_PART_MAIN, &list_btn_style);
//           // lv_page_glue_obj(list_btn, false);
//           // lv_obj_set_drag(list_btn, true);
//           // lv_obj_set_drag_throw(list_btn, true);
//         }
//       }
//       lv_obj_add_style (list1, LV_LIST_PART_BG, &style_list_btn);
//     }
// } filemgr_obj;

// static void dropdown_event_handler(lv_obj_t * obj, lv_event_t event)
// {
//   if(event == LV_EVENT_VALUE_CHANGED)
//   {
//     printf("Toggled\n");
//     std::string mediaFile  = audio_Files[lv_dropdown_get_selected(obj)]; // Use Element of mp class
//     std::cout << "Option: " <<  mediaFile << std::endl;
//     mp_obj.setMedia(mediaFile);
//     mp_gui_obj.setPlay();
//   }
// }


// void list_file_event_handler (lv_obj_t * obj, lv_event_t event)
// {
//   if (event == LV_EVENT_CLICKED) {
//     filemgr_obj.current_file = obj;
//     filemgr_obj.isdir = false;
//   }
// }

// void list_dir_event_handler (lv_obj_t * obj, lv_event_t event)
// {
//   if (event == LV_EVENT_CLICKED) {
//     filemgr_obj.current_dir = obj;
//     filemgr_obj.isdir = true;
//   }
//   if (event == LV_EVENT_DRAG_BEGIN) {
//     std::cout << "Godhelpme" << std::endl;
//   }
//   std::cout << "frickme" << std::endl;
// }

// /* TODO: Make files in selected directory into playlist */
// void list_sel_file_handler (lv_obj_t * obj, lv_event_t event)
// {
//   if (event == LV_EVENT_CLICKED) {
//     if (!filemgr_obj.isdir) {
//       int32_t index = lv_list_get_btn_index(NULL, filemgr_obj.current_file);
//       std::string file = filemgr_obj.files[index];
//       mp_obj.setMedia(file);
      
//     }
//   }
//   if (event == LV_EVENT_DRAG_THROW_BEGIN) {
//     lv_gesture_dir_t drag_dir = lv_indev_get_gesture_dir(mouse_indev);
//     // lv_point_t *pointi;
//     // lv_indev_get_vect(mouse_indev, pointi);
//     // std::cout << "Godhelpme " << drag_dir << std::endl;
//     std::cout << "Drag vector: " << mouse_indev->proc.types.pointer.vect.x << "/" << mouse_indev->proc.types.pointer.vect.y << std::endl;
//   }
//   if (event == LV_EVENT_DRAG_BEGIN) {
//     lv_gesture_dir_t drag_dir = lv_indev_get_gesture_dir(mouse_indev);
//     lv_point_t *pointi;
//     lv_indev_get_vect(mouse_indev, pointi);
//     std::cout << "Godhelpme " << drag_dir << std::endl;
//     lv_indev_wait_release(mouse_indev);
//     // std::cout << "Drag vector: " << mouse_indev->proc.types.pointer.vect.x << "/" << mouse_indev->proc.types.pointer.vect.y << std::endl;
//   }
// }

// void listdrag (lv_obj_t * obj, lv_event_t event)
// {
//   // std::cout << event << std::endl;
//   if (event == LV_EVENT_DRAG_THROW_BEGIN) {
//     std::cout << "Fuclgod" << std::endl;
//   }
// }

void onEndReached(void)
{
  std::cout << "EndReached" << std::endl;
  lv_label_set_text(mp_gui_obj.label, LV_SYMBOL_PLAY);  
}

void btn_event_handler(lv_obj_t * obj, lv_event_t event)
{
  if(event == LV_EVENT_VALUE_CHANGED)
  {
    printf("Toggled\n");
    auto plrState = mp_obj.plr.state();
    switch (plrState)
    {
    case libvlc_Playing:
      mp_obj.plr.pause();
      lv_label_set_text(mp_gui_obj.label, LV_SYMBOL_PLAY);
      break;

    case libvlc_Ended:
      mp_obj.plr.stop();
      mp_obj.plr.play();
      lv_label_set_text(mp_gui_obj.label, LV_SYMBOL_PAUSE);
      break;

    case libvlc_Error:
      std::cout << "Fuuuuuuuuuuuuuuck" << std::endl;
      lv_label_set_text(mp_gui_obj.label, LV_SYMBOL_WARNING);
      break;

    default:
      mp_obj.plr.play();
      lv_label_set_text(mp_gui_obj.label, LV_SYMBOL_PAUSE);
      break;
    }
  }
}

int main(int argc, char **argv)
{
  /*Initialize the Littlev graphics library*/  
  lv_init();

  /*Initialize the HAL (display, input devices, tick) for LVGL*/
  hal_init();
  // lv_ex_dropdown_1(); // Use Constructor
  mp_gui_obj.plb_btn();
  mp_gui_obj.create_ddl();
  // filemgr_obj.filelist();
  // lv_ex_imgbtn_1();

  while (1) {
    /* Periodically call the lv_task handler.
     * It could be done in a timer interrupt or an OS task too.*/
    lv_task_handler();
    usleep(5 * 1000);
  }
  return 0;
}


/**
 * Initialize the Hardware Abstraction Layer (HAL) for the Littlev graphics
 * library
 */
static void hal_init(void) {
  /* Use the 'monitor' driver which creates window on PC's monitor to simulate a display*/
  monitor_init();

  /*Create a display buffer*/
  static lv_disp_buf_t disp_buf1;
  static lv_color_t buf1_1[LV_HOR_RES_MAX * 120];
  lv_disp_buf_init(&disp_buf1, buf1_1, NULL, LV_HOR_RES_MAX * 120);

  /*Create a display*/
  lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv); /*Basic initialization*/
  disp_drv.buffer = &disp_buf1;
  disp_drv.flush_cb = monitor_flush;
  lv_disp_drv_register(&disp_drv);

  /* Add the mouse as input device
   * Use the 'mouse' driver which reads the PC's mouse*/
  mouse_init();
  lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv); /*Basic initialization*/
  indev_drv.type = LV_INDEV_TYPE_POINTER;

  /*This function will be called periodically (by the library) to get the mouse position and state*/
  indev_drv.read_cb = mouse_read;
  mouse_indev = lv_indev_drv_register(&indev_drv);

  /* Tick init.
   * You have to call 'lv_tick_inc()' in periodically to inform LittelvGL about
   * how much time were elapsed Create an SDL thread to do this*/
  SDL_CreateThread(tick_thread, "tick", NULL);

  /* Optional:
   * Create a memory monitor task which prints the memory usage in
   * periodically.*/
  //lv_task_create(memory_monitor, 5000, LV_TASK_PRIO_MID, NULL);
}

/**
 * A task to measure the elapsed time for LVGL
 * @param data unused
 * @return never return
 */
static int tick_thread(void *data) {
  (void)data;

  while (1) {
    SDL_Delay(5);   /*Sleep for 5 millisecond*/
    lv_tick_inc(5); /*Tell LittelvGL that 5 milliseconds were elapsed*/
  }

  return 0;
}

/**
 * Print the memory usage periodically
 * @param param
 */
static void memory_monitor(lv_task_t *param) {
  (void)param; /*Unused*/

  lv_mem_monitor_t mon;
  lv_mem_monitor(&mon);
  printf("used: %6d (%3d %%), frag: %3d %%, biggest free: %6d\n",
         (int)mon.total_size - mon.free_size, mon.used_pct, mon.frag_pct,
         (int)mon.free_biggest_size);
}
