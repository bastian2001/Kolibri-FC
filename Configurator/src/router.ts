import { createRouter, createWebHistory } from "vue-router"
import Home from "@views/Home.vue"
import Blackbox from "@views/Blackbox.vue"
import GpsMag from "@views/GpsMag.vue"
import Motors from "@views/Motors.vue"
import Osd from "@views/Osd.vue"
import Receiver from "@views/Receiver.vue"
import Tasks from "@views/Tasks.vue"
import Tuning from "@views/Tuning.vue"

export const routes = [
	{ path: "/", component: Home, name: "Home" },
	{ path: "/blackbox", component: Blackbox, name: "Blackbox" },
	{ path: "/gpsmag", component: GpsMag, name: "GPS & Mag" },
	{ path: "/motors", component: Motors, name: "Motors" },
	{ path: "/osd", component: Osd, name: "OSD" },
	{ path: "/receiver", component: Receiver, name: "Receiver" },
	{ path: "/tasks", component: Tasks, name: "Tasks" },
	{ path: "/tuning", component: Tuning, name: "Tuning" },
]

const router = createRouter({
	history: createWebHistory(),
	routes,
})

export default router
