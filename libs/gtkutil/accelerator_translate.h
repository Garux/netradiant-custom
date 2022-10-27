/*
   Copyright (C) 2001-2006, William Joseph.
   All Rights Reserved.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <Qt>

///\brief checks whether keyvalue is known in Qt::Key enum
///
inline bool qt_keyvalue_is_known( int keyvalue ){
    switch ( keyvalue ) {
        case Qt::Key::Key_Escape :                // misc keys
        case Qt::Key::Key_Tab :
        case Qt::Key::Key_Backtab :
        case Qt::Key::Key_Backspace :
        case Qt::Key::Key_Return :
        case Qt::Key::Key_Enter :
        case Qt::Key::Key_Insert :
        case Qt::Key::Key_Delete :
        case Qt::Key::Key_Pause :
        case Qt::Key::Key_Print :               // print screen
        case Qt::Key::Key_SysReq :
        case Qt::Key::Key_Clear :
        case Qt::Key::Key_Home :                // cursor movement
        case Qt::Key::Key_End :
        case Qt::Key::Key_Left :
        case Qt::Key::Key_Up :
        case Qt::Key::Key_Right :
        case Qt::Key::Key_Down :
        case Qt::Key::Key_PageUp :
        case Qt::Key::Key_PageDown :
        case Qt::Key::Key_Shift :                // modifiers
        case Qt::Key::Key_Control :
        case Qt::Key::Key_Meta :
        case Qt::Key::Key_Alt :
        case Qt::Key::Key_CapsLock :
        case Qt::Key::Key_NumLock :
        case Qt::Key::Key_ScrollLock :
        case Qt::Key::Key_F1 :                // function keys
        case Qt::Key::Key_F2 :
        case Qt::Key::Key_F3 :
        case Qt::Key::Key_F4 :
        case Qt::Key::Key_F5 :
        case Qt::Key::Key_F6 :
        case Qt::Key::Key_F7 :
        case Qt::Key::Key_F8 :
        case Qt::Key::Key_F9 :
        case Qt::Key::Key_F10 :
        case Qt::Key::Key_F11 :
        case Qt::Key::Key_F12 :
        case Qt::Key::Key_F13 :
        case Qt::Key::Key_F14 :
        case Qt::Key::Key_F15 :
        case Qt::Key::Key_F16 :
        case Qt::Key::Key_F17 :
        case Qt::Key::Key_F18 :
        case Qt::Key::Key_F19 :
        case Qt::Key::Key_F20 :
        case Qt::Key::Key_F21 :
        case Qt::Key::Key_F22 :
        case Qt::Key::Key_F23 :
        case Qt::Key::Key_F24 :
        case Qt::Key::Key_F25 :                // F25 .. F35 only on X11
        case Qt::Key::Key_F26 :
        case Qt::Key::Key_F27 :
        case Qt::Key::Key_F28 :
        case Qt::Key::Key_F29 :
        case Qt::Key::Key_F30 :
        case Qt::Key::Key_F31 :
        case Qt::Key::Key_F32 :
        case Qt::Key::Key_F33 :
        case Qt::Key::Key_F34 :
        case Qt::Key::Key_F35 :
        case Qt::Key::Key_Super_L :                 // extra keys
        case Qt::Key::Key_Super_R :
        case Qt::Key::Key_Menu :
        case Qt::Key::Key_Hyper_L :
        case Qt::Key::Key_Hyper_R :
        case Qt::Key::Key_Help :
        case Qt::Key::Key_Direction_L :
        case Qt::Key::Key_Direction_R :
        case Qt::Key::Key_Space :                // 7 bit printable ASCII
        case Qt::Key::Key_Exclam :
        case Qt::Key::Key_QuoteDbl :
        case Qt::Key::Key_NumberSign :
        case Qt::Key::Key_Dollar :
        case Qt::Key::Key_Percent :
        case Qt::Key::Key_Ampersand :
        case Qt::Key::Key_Apostrophe :
        case Qt::Key::Key_ParenLeft :
        case Qt::Key::Key_ParenRight :
        case Qt::Key::Key_Asterisk :
        case Qt::Key::Key_Plus :
        case Qt::Key::Key_Comma :
        case Qt::Key::Key_Minus :
        case Qt::Key::Key_Period :
        case Qt::Key::Key_Slash :
        case Qt::Key::Key_0 :
        case Qt::Key::Key_1 :
        case Qt::Key::Key_2 :
        case Qt::Key::Key_3 :
        case Qt::Key::Key_4 :
        case Qt::Key::Key_5 :
        case Qt::Key::Key_6 :
        case Qt::Key::Key_7 :
        case Qt::Key::Key_8 :
        case Qt::Key::Key_9 :
        case Qt::Key::Key_Colon :
        case Qt::Key::Key_Semicolon :
        case Qt::Key::Key_Less :
        case Qt::Key::Key_Equal :
        case Qt::Key::Key_Greater :
        case Qt::Key::Key_Question :
        case Qt::Key::Key_At :
        case Qt::Key::Key_A :
        case Qt::Key::Key_B :
        case Qt::Key::Key_C :
        case Qt::Key::Key_D :
        case Qt::Key::Key_E :
        case Qt::Key::Key_F :
        case Qt::Key::Key_G :
        case Qt::Key::Key_H :
        case Qt::Key::Key_I :
        case Qt::Key::Key_J :
        case Qt::Key::Key_K :
        case Qt::Key::Key_L :
        case Qt::Key::Key_M :
        case Qt::Key::Key_N :
        case Qt::Key::Key_O :
        case Qt::Key::Key_P :
        case Qt::Key::Key_Q :
        case Qt::Key::Key_R :
        case Qt::Key::Key_S :
        case Qt::Key::Key_T :
        case Qt::Key::Key_U :
        case Qt::Key::Key_V :
        case Qt::Key::Key_W :
        case Qt::Key::Key_X :
        case Qt::Key::Key_Y :
        case Qt::Key::Key_Z :
        case Qt::Key::Key_BracketLeft :
        case Qt::Key::Key_Backslash :
        case Qt::Key::Key_BracketRight :
        case Qt::Key::Key_AsciiCircum :
        case Qt::Key::Key_Underscore :
        case Qt::Key::Key_QuoteLeft :
        case Qt::Key::Key_BraceLeft :
        case Qt::Key::Key_Bar :
        case Qt::Key::Key_BraceRight :
        case Qt::Key::Key_AsciiTilde :

        case Qt::Key::Key_nobreakspace :
        case Qt::Key::Key_exclamdown :
        case Qt::Key::Key_cent :
        case Qt::Key::Key_sterling :
        case Qt::Key::Key_currency :
        case Qt::Key::Key_yen :
        case Qt::Key::Key_brokenbar :
        case Qt::Key::Key_section :
        case Qt::Key::Key_diaeresis :
        case Qt::Key::Key_copyright :
        case Qt::Key::Key_ordfeminine :
        case Qt::Key::Key_guillemotleft :        // left angle quotation mark
        case Qt::Key::Key_notsign :
        case Qt::Key::Key_hyphen :
        case Qt::Key::Key_registered :
        case Qt::Key::Key_macron :
        case Qt::Key::Key_degree :
        case Qt::Key::Key_plusminus :
        case Qt::Key::Key_twosuperior :
        case Qt::Key::Key_threesuperior :
        case Qt::Key::Key_acute :
        case Qt::Key::Key_mu :
        case Qt::Key::Key_paragraph :
        case Qt::Key::Key_periodcentered :
        case Qt::Key::Key_cedilla :
        case Qt::Key::Key_onesuperior :
        case Qt::Key::Key_masculine :
        case Qt::Key::Key_guillemotright :        // right angle quotation mark
        case Qt::Key::Key_onequarter :
        case Qt::Key::Key_onehalf :
        case Qt::Key::Key_threequarters :
        case Qt::Key::Key_questiondown :
        case Qt::Key::Key_Agrave :
        case Qt::Key::Key_Aacute :
        case Qt::Key::Key_Acircumflex :
        case Qt::Key::Key_Atilde :
        case Qt::Key::Key_Adiaeresis :
        case Qt::Key::Key_Aring :
        case Qt::Key::Key_AE :
        case Qt::Key::Key_Ccedilla :
        case Qt::Key::Key_Egrave :
        case Qt::Key::Key_Eacute :
        case Qt::Key::Key_Ecircumflex :
        case Qt::Key::Key_Ediaeresis :
        case Qt::Key::Key_Igrave :
        case Qt::Key::Key_Iacute :
        case Qt::Key::Key_Icircumflex :
        case Qt::Key::Key_Idiaeresis :
        case Qt::Key::Key_ETH :
        case Qt::Key::Key_Ntilde :
        case Qt::Key::Key_Ograve :
        case Qt::Key::Key_Oacute :
        case Qt::Key::Key_Ocircumflex :
        case Qt::Key::Key_Otilde :
        case Qt::Key::Key_Odiaeresis :
        case Qt::Key::Key_multiply :
        case Qt::Key::Key_Ooblique :
        case Qt::Key::Key_Ugrave :
        case Qt::Key::Key_Uacute :
        case Qt::Key::Key_Ucircumflex :
        case Qt::Key::Key_Udiaeresis :
        case Qt::Key::Key_Yacute :
        case Qt::Key::Key_THORN :
        case Qt::Key::Key_ssharp :
        case Qt::Key::Key_division :
        case Qt::Key::Key_ydiaeresis :

        // International input method support (X keycode - 0xEE00, the
        // definition follows Qt/Embedded 2.3.7) Only interesting if
        // you are writing your own input method

        // International & multi-key character composition
        case Qt::Key::Key_AltGr               :
        case Qt::Key::Key_Multi_key           :  // Multi-key character compose
        case Qt::Key::Key_Codeinput           :
        case Qt::Key::Key_SingleCandidate     :
        case Qt::Key::Key_MultipleCandidate   :
        case Qt::Key::Key_PreviousCandidate   :

        // Misc Functions
        case Qt::Key::Key_Mode_switch         :  // Character set switch
        //case Qt::Key::Key_script_switch       :  // Alias for mode_switch

        // Japanese keyboard support
        case Qt::Key::Key_Kanji               :  // Kanji, Kanji convert
        case Qt::Key::Key_Muhenkan            :  // Cancel Conversion
        //case Qt::Key::Key_Henkan_Mode         :  // Start/Stop Conversion
        case Qt::Key::Key_Henkan              :  // Alias for Henkan_Mode
        case Qt::Key::Key_Romaji              :  // to Romaji
        case Qt::Key::Key_Hiragana            :  // to Hiragana
        case Qt::Key::Key_Katakana            :  // to Katakana
        case Qt::Key::Key_Hiragana_Katakana   :  // Hiragana/Katakana toggle
        case Qt::Key::Key_Zenkaku             :  // to Zenkaku
        case Qt::Key::Key_Hankaku             :  // to Hankaku
        case Qt::Key::Key_Zenkaku_Hankaku     :  // Zenkaku/Hankaku toggle
        case Qt::Key::Key_Touroku             :  // Add to Dictionary
        case Qt::Key::Key_Massyo              :  // Delete from Dictionary
        case Qt::Key::Key_Kana_Lock           :  // Kana Lock
        case Qt::Key::Key_Kana_Shift          :  // Kana Shift
        case Qt::Key::Key_Eisu_Shift          :  // Alphanumeric Shift
        case Qt::Key::Key_Eisu_toggle         :  // Alphanumeric toggle
        //case Qt::Key::Key_Kanji_Bangou        :  // Codeinput
        //case Qt::Key::Key_Zen_Koho            :  // Multiple/All Candidate(s)
        //case Qt::Key::Key_Mae_Koho            :  // Previous Candidate

        // Korean keyboard support
        //
        // In fact, many Korean users need only 2 keys, Key_Hangul and
        // Key_Hangul_Hanja. But rest of the keys are good for future.

        case Qt::Key::Key_Hangul              :  // Hangul start/stop(toggle)
        case Qt::Key::Key_Hangul_Start        :  // Hangul start
        case Qt::Key::Key_Hangul_End          :  // Hangul end, English start
        case Qt::Key::Key_Hangul_Hanja        :  // Start Hangul->Hanja Conversion
        case Qt::Key::Key_Hangul_Jamo         :  // Hangul Jamo mode
        case Qt::Key::Key_Hangul_Romaja       :  // Hangul Romaja mode
        //case Qt::Key::Key_Hangul_Codeinput    :  // Hangul code input mode
        case Qt::Key::Key_Hangul_Jeonja       :  // Jeonja mode
        case Qt::Key::Key_Hangul_Banja        :  // Banja mode
        case Qt::Key::Key_Hangul_PreHanja     :  // Pre Hanja conversion
        case Qt::Key::Key_Hangul_PostHanja    :  // Post Hanja conversion
        //case Qt::Key::Key_Hangul_SingleCandidate   :  // Single candidate
        //case Qt::Key::Key_Hangul_MultipleCandidate :  // Multiple candidate
        //case Qt::Key::Key_Hangul_PreviousCandidate :  // Previous candidate
        case Qt::Key::Key_Hangul_Special      :  // Special symbols
        //case Qt::Key::Key_Hangul_switch       :  // Alias for mode_switch

        // dead keys (X keycode - 0xED00 to avoid the conflict)
        case Qt::Key::Key_Dead_Grave          :
        case Qt::Key::Key_Dead_Acute          :
        case Qt::Key::Key_Dead_Circumflex     :
        case Qt::Key::Key_Dead_Tilde          :
        case Qt::Key::Key_Dead_Macron         :
        case Qt::Key::Key_Dead_Breve          :
        case Qt::Key::Key_Dead_Abovedot       :
        case Qt::Key::Key_Dead_Diaeresis      :
        case Qt::Key::Key_Dead_Abovering      :
        case Qt::Key::Key_Dead_Doubleacute    :
        case Qt::Key::Key_Dead_Caron          :
        case Qt::Key::Key_Dead_Cedilla        :
        case Qt::Key::Key_Dead_Ogonek         :
        case Qt::Key::Key_Dead_Iota           :
        case Qt::Key::Key_Dead_Voiced_Sound   :
        case Qt::Key::Key_Dead_Semivoiced_Sound :
        case Qt::Key::Key_Dead_Belowdot       :
        case Qt::Key::Key_Dead_Hook           :
        case Qt::Key::Key_Dead_Horn           :
        case Qt::Key::Key_Dead_Stroke         :
        case Qt::Key::Key_Dead_Abovecomma     :
        case Qt::Key::Key_Dead_Abovereversedcomma :
        case Qt::Key::Key_Dead_Doublegrave    :
        case Qt::Key::Key_Dead_Belowring      :
        case Qt::Key::Key_Dead_Belowmacron    :
        case Qt::Key::Key_Dead_Belowcircumflex :
        case Qt::Key::Key_Dead_Belowtilde     :
        case Qt::Key::Key_Dead_Belowbreve     :
        case Qt::Key::Key_Dead_Belowdiaeresis :
        case Qt::Key::Key_Dead_Invertedbreve  :
        case Qt::Key::Key_Dead_Belowcomma     :
        case Qt::Key::Key_Dead_Currency       :
        case Qt::Key::Key_Dead_a              :
        case Qt::Key::Key_Dead_A              :
        case Qt::Key::Key_Dead_e              :
        case Qt::Key::Key_Dead_E              :
        case Qt::Key::Key_Dead_i              :
        case Qt::Key::Key_Dead_I              :
        case Qt::Key::Key_Dead_o              :
        case Qt::Key::Key_Dead_O              :
        case Qt::Key::Key_Dead_u              :
        case Qt::Key::Key_Dead_U              :
        case Qt::Key::Key_Dead_Small_Schwa    :
        case Qt::Key::Key_Dead_Capital_Schwa  :
        case Qt::Key::Key_Dead_Greek          :
        case Qt::Key::Key_Dead_Lowline        :
        case Qt::Key::Key_Dead_Aboveverticalline :
        case Qt::Key::Key_Dead_Belowverticalline :
        case Qt::Key::Key_Dead_Longsolidusoverlay :

        // multimedia/internet keys - ignored by default - see QKeyEvent c'tor
        case Qt::Key::Key_Back  :
        case Qt::Key::Key_Forward  :
        case Qt::Key::Key_Stop  :
        case Qt::Key::Key_Refresh  :
        case Qt::Key::Key_VolumeDown :
        case Qt::Key::Key_VolumeMute  :
        case Qt::Key::Key_VolumeUp :
        case Qt::Key::Key_BassBoost :
        case Qt::Key::Key_BassUp :
        case Qt::Key::Key_BassDown :
        case Qt::Key::Key_TrebleUp :
        case Qt::Key::Key_TrebleDown :
        case Qt::Key::Key_MediaPlay  :
        case Qt::Key::Key_MediaStop  :
        case Qt::Key::Key_MediaPrevious  :
        case Qt::Key::Key_MediaNext  :
        case Qt::Key::Key_MediaRecord :
        case Qt::Key::Key_MediaPause :
        case Qt::Key::Key_MediaTogglePlayPause :
        case Qt::Key::Key_HomePage  :
        case Qt::Key::Key_Favorites  :
        case Qt::Key::Key_Search  :
        case Qt::Key::Key_Standby :
        case Qt::Key::Key_OpenUrl :
        case Qt::Key::Key_LaunchMail  :
        case Qt::Key::Key_LaunchMedia :
        case Qt::Key::Key_Launch0  :
        case Qt::Key::Key_Launch1  :
        case Qt::Key::Key_Launch2  :
        case Qt::Key::Key_Launch3  :
        case Qt::Key::Key_Launch4  :
        case Qt::Key::Key_Launch5  :
        case Qt::Key::Key_Launch6  :
        case Qt::Key::Key_Launch7  :
        case Qt::Key::Key_Launch8  :
        case Qt::Key::Key_Launch9  :
        case Qt::Key::Key_LaunchA  :
        case Qt::Key::Key_LaunchB  :
        case Qt::Key::Key_LaunchC  :
        case Qt::Key::Key_LaunchD  :
        case Qt::Key::Key_LaunchE  :
        case Qt::Key::Key_LaunchF  :
        case Qt::Key::Key_MonBrightnessUp :
        case Qt::Key::Key_MonBrightnessDown :
        case Qt::Key::Key_KeyboardLightOnOff :
        case Qt::Key::Key_KeyboardBrightnessUp :
        case Qt::Key::Key_KeyboardBrightnessDown :
        case Qt::Key::Key_PowerOff :
        case Qt::Key::Key_WakeUp :
        case Qt::Key::Key_Eject :
        case Qt::Key::Key_ScreenSaver :
        case Qt::Key::Key_WWW :
        case Qt::Key::Key_Memo :
        case Qt::Key::Key_LightBulb :
        case Qt::Key::Key_Shop :
        case Qt::Key::Key_History :
        case Qt::Key::Key_AddFavorite :
        case Qt::Key::Key_HotLinks :
        case Qt::Key::Key_BrightnessAdjust :
        case Qt::Key::Key_Finance :
        case Qt::Key::Key_Community :
        case Qt::Key::Key_AudioRewind : // Media rewind
        case Qt::Key::Key_BackForward :
        case Qt::Key::Key_ApplicationLeft :
        case Qt::Key::Key_ApplicationRight :
        case Qt::Key::Key_Book :
        case Qt::Key::Key_CD :
        case Qt::Key::Key_Calculator :
        case Qt::Key::Key_ToDoList :
        case Qt::Key::Key_ClearGrab :
        case Qt::Key::Key_Close :
        case Qt::Key::Key_Copy :
        case Qt::Key::Key_Cut :
        case Qt::Key::Key_Display : // Output switch key
        case Qt::Key::Key_DOS :
        case Qt::Key::Key_Documents :
        case Qt::Key::Key_Excel :
        case Qt::Key::Key_Explorer :
        case Qt::Key::Key_Game :
        case Qt::Key::Key_Go :
        case Qt::Key::Key_iTouch :
        case Qt::Key::Key_LogOff :
        case Qt::Key::Key_Market :
        case Qt::Key::Key_Meeting :
        case Qt::Key::Key_MenuKB :
        case Qt::Key::Key_MenuPB :
        case Qt::Key::Key_MySites :
        case Qt::Key::Key_News :
        case Qt::Key::Key_OfficeHome :
        case Qt::Key::Key_Option :
        case Qt::Key::Key_Paste :
        case Qt::Key::Key_Phone :
        case Qt::Key::Key_Calendar :
        case Qt::Key::Key_Reply :
        case Qt::Key::Key_Reload :
        case Qt::Key::Key_RotateWindows :
        case Qt::Key::Key_RotationPB :
        case Qt::Key::Key_RotationKB :
        case Qt::Key::Key_Save :
        case Qt::Key::Key_Send :
        case Qt::Key::Key_Spell :
        case Qt::Key::Key_SplitScreen :
        case Qt::Key::Key_Support :
        case Qt::Key::Key_TaskPane :
        case Qt::Key::Key_Terminal :
        case Qt::Key::Key_Tools :
        case Qt::Key::Key_Travel :
        case Qt::Key::Key_Video :
        case Qt::Key::Key_Word :
        case Qt::Key::Key_Xfer :
        case Qt::Key::Key_ZoomIn :
        case Qt::Key::Key_ZoomOut :
        case Qt::Key::Key_Away :
        case Qt::Key::Key_Messenger :
        case Qt::Key::Key_WebCam :
        case Qt::Key::Key_MailForward :
        case Qt::Key::Key_Pictures :
        case Qt::Key::Key_Music :
        case Qt::Key::Key_Battery :
        case Qt::Key::Key_Bluetooth :
        case Qt::Key::Key_WLAN :
        case Qt::Key::Key_UWB :
        case Qt::Key::Key_AudioForward : // Media fast-forward
        case Qt::Key::Key_AudioRepeat : // Toggle repeat mode
        case Qt::Key::Key_AudioRandomPlay : // Toggle shuffle mode
        case Qt::Key::Key_Subtitle :
        case Qt::Key::Key_AudioCycleTrack :
        case Qt::Key::Key_Time :
        case Qt::Key::Key_Hibernate :
        case Qt::Key::Key_View :
        case Qt::Key::Key_TopMenu :
        case Qt::Key::Key_PowerDown :
        case Qt::Key::Key_Suspend :
        case Qt::Key::Key_ContrastAdjust :

        case Qt::Key::Key_LaunchG  :
        case Qt::Key::Key_LaunchH  :

        case Qt::Key::Key_TouchpadToggle :
        case Qt::Key::Key_TouchpadOn :
        case Qt::Key::Key_TouchpadOff :

        case Qt::Key::Key_MicMute :

        case Qt::Key::Key_Red :
        case Qt::Key::Key_Green :
        case Qt::Key::Key_Yellow :
        case Qt::Key::Key_Blue :

        case Qt::Key::Key_ChannelUp :
        case Qt::Key::Key_ChannelDown :

        case Qt::Key::Key_Guide    :
        case Qt::Key::Key_Info     :
        case Qt::Key::Key_Settings :

        case Qt::Key::Key_MicVolumeUp   :
        case Qt::Key::Key_MicVolumeDown :

        case Qt::Key::Key_New      :
        case Qt::Key::Key_Open     :
        case Qt::Key::Key_Find     :
        case Qt::Key::Key_Undo     :
        case Qt::Key::Key_Redo     :

        case Qt::Key::Key_MediaLast :

        // Keypad navigation keys
        case Qt::Key::Key_Select :
        case Qt::Key::Key_Yes :
        case Qt::Key::Key_No :

        // Newer misc keys
        case Qt::Key::Key_Cancel  :
        case Qt::Key::Key_Printer :
        case Qt::Key::Key_Execute :
        case Qt::Key::Key_Sleep   :
        case Qt::Key::Key_Play    : // Not the same as Key_MediaPlay
        case Qt::Key::Key_Zoom    :
        //case Qt::Key::Key_Jisho   : // IME: Dictionary key
        //case Qt::Key::Key_Oyayubi_Left : // IME: Left Oyayubi key
        //case Qt::Key::Key_Oyayubi_Right : // IME: Right Oyayubi key
        case Qt::Key::Key_Exit    :

        // Device keys
        case Qt::Key::Key_Context1 :
        case Qt::Key::Key_Context2 :
        case Qt::Key::Key_Context3 :
        case Qt::Key::Key_Context4 :
        case Qt::Key::Key_Call :      // set absolute state to in a call (do not toggle state)
        case Qt::Key::Key_Hangup :    // set absolute state to hang up (do not toggle state)
        case Qt::Key::Key_Flip :
        case Qt::Key::Key_ToggleCallHangup : // a toggle key for answering, or hanging up, based on current call state
        case Qt::Key::Key_VoiceDial :
        case Qt::Key::Key_LastNumberRedial :

        case Qt::Key::Key_Camera :
        case Qt::Key::Key_CameraFocus :

        case Qt::Key::Key_unknown :
			return true;
		default:
			return false;
	}
}