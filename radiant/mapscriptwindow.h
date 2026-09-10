#pragma once

class QMainWindow;

bool MapScripts_isSupportedGame();
void MapScripts_constructWindow( QMainWindow* main_window );
void MapScripts_destroyWindow();
void MapScripts_toggleShown();
