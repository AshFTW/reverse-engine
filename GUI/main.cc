/*
    This file is part of Reverse Engine.

    Driver GUI.

    Copyright (C) 2017-2018 Ivan Stepanov <ivanstepanovftw@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "MainWindow.hh"

std::shared_ptr<RE::globals_t> RE::globals;

int main(int argc, char *argv[]) {
    RE::globals = std::make_shared<RE::globals_t>();

    auto app = Gtk::Application::create(argc, argv, "com.github.ivanstepanovftw.reverse-engine");
    //Gdl::init();
    
    MainWindow window;
    //window.set_keep_above(true);
    //window.set_accept_focus(false);
    return app->run(window);
}
