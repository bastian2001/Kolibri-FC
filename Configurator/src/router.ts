import { createRouter, createWebHistory } from "vue-router"
import Home from "@views/Home.vue"
import Blackbox from "@views/Blackbox.vue"
import GpsMag from "@views/GpsMag.vue"
import Motors from "@views/Motors.vue"
import Osd from "@views/Osd.vue"
import Receiver from "@views/Receiver.vue"
import Tasks from "@views/Tasks.vue"
import Tuning from "@views/Tuning.vue"
import Cli from "@views/Cli.vue"
import Sensors from "@views/Sensors.vue"
import Battery from "./views/Battery.vue"

export const routes = [
	{ path: "/", component: Home, name: "Home" },
	{ path: "/blackbox", component: Blackbox, name: "Blackbox" },
	{ path: "/tasks", component: Tasks, name: "Tasks" },
	{ path: "/osd", component: Osd, name: "OSD" },
	{ path: "/motors", component: Motors, name: "Motors" },
	{ path: "/battery", component: Battery, name: "Battery" },
	{ path: "/receiver", component: Receiver, name: "Receiver" },
	{ path: "/sensors", component: Sensors, name: "Sensors" },
	{ path: "/tuning", component: Tuning, name: "Tuning" },
	{ path: "/gpsmag", component: GpsMag, name: "GPS & Mag" },
	{ path: "/cli", component: Cli, name: "CLI" },
]

const router = createRouter({
	history: createWebHistory(),
	routes,
})

export default router
