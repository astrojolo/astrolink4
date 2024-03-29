# indi-astrolink4
astrolink4 INDI driver supports [AstroLink 4 mini device](https://astrojolo.com/astrolink-4-0-mini/). AstroLink 4.0 mini device was designed to make astroimaging easier. Contains two focuser controllers, regulated outputs for heaters, peltiers or fans, switchable power outputs, different sensors inputs, hand controller connector and many other options. Following funcitons are supported in INDI driver:
- 1x stepper focuser motor output
- 1x DC focuser motor output
- 3x switchable 12V power outputs
- 2x regulated PWM outputs
- temperature / humidity / dew point readings
- voltage, current, consumed energy monitoring

# Installing INDI server and libraries
To start you need to download and install INDI environment. See [INDI page](http://indilib.org/download.html) for details. 

Then AstroLink INDI driver needs to be fetched and installed:

```
git clone https://github.com/astrojolo/astrolink4.git
cd astrolink4
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
make
sudo make install
```

Then indiserver needs to be started with AstroLink drivers:

```
indiserver -v indi_astrolink4
```

Now AstroLink can be used with any software that supports INDI drivers, like KStars with Ekos.

<a href="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-connection.png"><img src="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-connection.png" width="400" ></a><a href="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-options.png"><img src="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-options.png" width="400" ></a>
<br />
<a href="https://astrojolo.com/wp-content/uploads/2019/10/astrolink-indi-focuser.jpg"><img src="https://astrojolo.com/wp-content/uploads/2019/10/astrolink-indi-focuser.jpg" width="400" ></a><a href="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-environment.png"><img src="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-environment.png" width="400" ></a>
<br />
<a href="https://astrojolo.com/wp-content/uploads/2019/10/astrolink-indi-power.jpg"><img src="https://astrojolo.com/wp-content/uploads/2019/10/astrolink-indi-power.jpg" width="400" ></a><a href="https://astrojolo.com/wp-content/uploads/2019/10/astrolink-indi-settings.jpg"><img src="https://astrojolo.com/wp-content/uploads/2019/10/astrolink-indi-settings.jpg" width="400" ></a>
<br />
<a href="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-dcfocuser.png"><img src="https://astrojolo.com/wp-content/uploads/2020/04/astrolink-indi-dcfocuser.png" width="400" ></a>
