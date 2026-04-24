const { log } = require("console")
const fs = require("fs")

//if a drive with letter G appears, copy .pio/build/pico/firmware.uf2 to it

const DRIVE = "Q:\\"
const CYCLETIME = 200
const TIMEOUT = 5000
const RETRYTIME = 2000

console.log("Waiting for drive " + DRIVE + " to appear")
console.log("It is possible to set the drive letter in upload.js")
console.log("Use diskmgmt.msc to set a static drive letter for the pico")

let recheck = true
setInterval(() => {
	if (recheck) {
		fs.access(DRIVE, err => {
			if (err === null) {
				//upload
				fs.copyFile(".pio/build/pico/firmware.uf2", DRIVE + "firmware.uf2", err => {
					if (err) {
						console.error(err)
						console.log("Failed to upload, try again in " + RETRYTIME / 1000 + " seconds")
						setTimeout(() => {
							recheck = true
						}, RETRYTIME)
						return
					}
					//print success with a timestamp
					console.log("Uploaded", new Date().toLocaleTimeString())
				})
				recheck = false
				setTimeout(() => {
					recheck = true
				}, TIMEOUT)
			} else recheck = true
		})
	}
}, CYCLETIME)
