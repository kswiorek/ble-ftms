import tkinter as tk
from tkinter import ttk
from tkinter.font import Font

root = tk.Tk()  

width = 800
height= 600 

gears = [   0.323529412,
	0.382352941,
	0.441176471,
	0.458333333,
	0.5		   ,
	0.541666667,
	0.558823529,
	0.617647059,
	0.625	   ,
	0.705882353,		 
	0.708333333,
	0.791666667,
	0.823529412,
	0.875	   ,
	0.941176471,
	1		   ,
	1.088235294,
	1.166666667,
	1.235294118,
	1.333333333,
	1.541666667,
	1.75]

#gears = [1,2,3,4]

gear_num = len(gears)
level_settings = 20

tab = ttk.Notebook(root)
tab.pack()

frame1 = tk.Frame(tab)
canvas1 = tk.Canvas(frame1,width=width,height=height, background='white')

label1 = tk.Text( frame1, width=20, height=37)
label1.insert(.0, 'int gearNum = ' + str(len(gears))+'\nfloat gears = {\n'+str(gears).replace(', ', ', \n')[1:-1]+'}')

scale = (max(gears)+1)

for i in range(int(width/(gear_num)), width+20, int((width)/gear_num)):
    canvas1.create_line(i, 0, i, height)
    t = canvas1.create_text(i, height-20, font="Arial", text=str(int(i/int(width/(gear_num)))))
    r=canvas1.create_rectangle(canvas1.bbox(t),fill="white", outline='white')
    canvas1.tag_lower(r,t)
    
for i in range(40, height-20, int(height/scale)):
    canvas1.create_line(0, height-i, width, height-i)
    t = canvas1.create_text(20, height-i, font="Arial", text=str(int(i/int(height/scale))))
    r=canvas1.create_rectangle(canvas1.bbox(t),fill="white", outline='white')
    canvas1.tag_lower(r,t)
    
def move_gears(event):
    canvas1_item_id = event.widget.find_withtag('current')[0]
    gears[p.index(canvas1_item_id)] = (-event.y + height-40)/(height/scale)
    x[p1], y1, x[p2], y2  = canvas1.coords(canvas1_item_id)
    canvas1.coords(canvas1_item_id, x[p1], height-(height/scale)*gears[p.index(canvas1_item_id)]-5-40, x[p2], height-(height/scale)*gears[p.index(canvas1_item_id)]+5-40)
    label1.delete(1.0, tk.END)
    label1.insert(.0, 'int gearNum = ' + str(len(gears))+'\nfloat gears = {\n'+str(gears).replace(', ', ', \n')[1:-1]+'}')
    
p=[]    
def draw(gears):
    for i in range(int(width/(gear_num)), width+20, int((width)/gear_num)):
       p.append(canvas1.create_oval(i-5, height-(height/scale)*gears[int(i/(width/(gear_num)))]-5-40, i+5, height-(height/scale)*gears[int(i/(width/(gear_num)))]+5-40, fill="black", outline='black'))
    for i in p:
        canvas1.tag_bind(i, '<B1-Motion>', move_gears)       
draw(gears)

label1.pack(side='right')
canvas1.pack()
frame1.pack()

frame2 = tk.Frame(tab)
canvas2 = tk.Canvas(frame2,width=width,height=height, background='white')
    

label2 = tk.Text( frame2, width=20, height=3)

var = tk.StringVar()

slider = tk.Scale( frame2, resolution=0.01, variable = var, from_=-10, to=10, digits=4, orient=tk.HORIZONTAL, sliderlength = 20, length = 160)


start = (40, 40)
end = (width-20, height-20)

for i in range(0, level_settings+1):
    x = start[0]+((end[0]-start[0])/(level_settings))*i
    canvas2.create_line(x, 0, x, height)
    t=canvas2.create_text(x, height-(start[1]-20), font="Arial", text=i)
    r=canvas2.create_rectangle(canvas2.bbox(t),fill="white", outline='white')
    canvas2.tag_lower(r,t)
    
gradient_range = 20

for i in range(0, gradient_range+2):
    y = ((end[1]-start[1])/(gradient_range))*i + start[1]
    canvas2.create_line(0, height-y, width, height-y)
    t=canvas2.create_text(start[0]-20, height-y, font="Arial", text=i)
    r=canvas2.create_rectangle(canvas2.bbox(t),fill="white", outline='white')
    canvas2.tag_lower(r,t)
    
gear = 0
coeff1 = 0.7492
coeff2 = 1.734
coeff3 = -0.1076
a=coeff1*gears[gear]
b=coeff2*gears[gear]+coeff3
slider.set(coeff2)
label2.insert(.0, '#define c1 {}\n#define c2 {}\n#define c3 {}'.format(round(coeff1,3), round(coeff2,3), round(coeff3,3)))

def convert(point):
    return start[0]+((end[0]-start[0])/(level_settings))*point[1], height-(((end[1]-start[1])/(gradient_range))*point[0] + start[1])
def deconvert(point):
    return (height-point[1]-start[1])/((end[1]-start[1])/(gradient_range)), (point[0]-start[0])/((end[0]-start[0])/(level_settings))

point1 = (0, a*0+b)
point2 = (gradient_range, a*gradient_range+b)

point1w = convert(point1)
point2w = convert(point2)

line = canvas2.create_line(point1w, point2w, width=3, fill='red')

p1 = canvas2.create_oval(0,0,0,0, fill="black", outline='black')
p2 = canvas2.create_oval(0,0,0,0, fill="black", outline='black')

x = {p1:2,
     p2:4}

point1o = (x[p1], a*x[p1]+b)
point2o = (x[p2], a*x[p2]+b)

point = {p1:convert(point1o),
         p2:convert(point1o)}

canvas2.coords(p1, point[p1][0]-5, point[p1][1]-5, point[p1][0]+5, point[p1][1]+5)
canvas2.coords(p2, point[p2][0]-5, point[p2][1]-5, point[p2][0]+5, point[p2][1]+5)

    
def callback(sv):
    global gear
    gear = int(sv.get())
    global a
    global b
    global coeff1
    global coeff2
    global coeff3
    
    a=coeff1*gears[gear]
    b=coeff2*gears[gear]+coeff3    
    
    point1 = (0, a*0+b)
    point2 = (gradient_range, a*gradient_range+b)
    
    point1w = convert(point1)
    point2w = convert(point2)
    canvas2.coords(line, point1w[0], point1w[1], point2w[0], point2w[1])
    
    if a*x[p1]+b >= deconvert((width,0))[1]:
        point[p1] = ((deconvert((width,0))[1]-b)/a, deconvert((width,0))[1])
        x[p1] = (deconvert((width,0))[1]-b)/a
    else:
        point[p1] = (x[p1], a*x[p1]+b)
        
    if a*x[p2]+b >= deconvert((width,0))[1]:
        point[p2] = ((deconvert((width,0))[1]-b)/a, deconvert((width,0))[1])
        x[p2] = (deconvert((width,0))[1]-b)/a
    else:
        point[p2] = (x[p2], a*x[p2]+b)
    
    point1o = convert(point[p1])
    point2o = convert(point[p2])
    
    
    canvas2.coords(p1, point1o[0]-5, point1o[1]-5, point1o[0]+5, point1o[1]+5)
    canvas2.coords(p2, point2o[0]-5, point2o[1]-5, point2o[0]+5, point2o[1]+5)
    label2.delete(1.0, tk.END)
    label2.insert(.0, '#define c1 {}\n#define c2 {}\n#define c3 {}'.format(round(coeff1,3), round(coeff2,3), round(coeff3,3)))
    
def point_move(event):
    mouse_x = event.x if event.x<=width else width
    mouse_y = event.y if event.y<=height else height
    
    mouse_x = 0 if event.x<=0 else mouse_x
    mouse_y = 0 if event.y<=0 else mouse_y
        
    
    item = event.widget.find_withtag('current')[0]
    canvas2.coords(item, mouse_x-5, mouse_y-5, mouse_x+5, mouse_y+5)
    point[item] = (height-mouse_y-start[1])/((end[1]-start[1])/(gradient_range)), (mouse_x-start[0])/((end[0]-start[0])/(level_settings))
    x[item]=point[item][0]
    global a
    global b
    a = (point[p1][1]-point[p2][1])/(point[p1][0]-point[p2][0])

    b = point[p1][1]-a*point[p1][0]
    global coeff1
    global coeff2
    global coeff3
    coeff1=a/gears[gear]
    coeff3=b-coeff2*gears[gear]  
    
    point1 = (0, a*0+b)
    point2 = (gradient_range, a*gradient_range+b)
    
    point1w = convert(point1)
    point2w = convert(point2)
    canvas2.coords(line, point1w[0], point1w[1], point2w[0], point2w[1])
    label2.delete(1.0, tk.END)
    label2.insert(.0, '#define c1 {}\n#define c2 {}\n#define c3 {}'.format(round(coeff1,3), round(coeff2,3), round(coeff3,3)))
    
canvas2.tag_bind(p1, '<B1-Motion>', point_move)
canvas2.tag_bind(p2, '<B1-Motion>', point_move)
sv = tk.StringVar()

def refresh_coeff(coeff):
    global coeff2
    global coeff3
    coeff2 = float(var.get())
    coeff3 = b-coeff2*gears[gear]  
    label2.delete(1.0, tk.END)
    label2.insert(.0, '#define c1 {}\n#define c2 {}\n#define c3 {}'.format(round(coeff1,3), round(coeff2,3), round(coeff3,3)))

sv.trace("w", lambda name, index, mode, sv=sv: callback(sv))
var.trace("w", lambda name, index, mode, var=var: refresh_coeff(var))
    
spin = tk.Spinbox(frame2, textvariable=sv, from_=0, to=len(gears)-1, width=2, font=Font(size=20))

txt = tk.Label(frame2, text='Gear:', font=Font(size=20))

canvas2.pack(anchor='w')
spin.place(x=width+75, y=0)
txt.place(x=width, y=0)
label2.place(x=width, y=40)
slider.place(x=width, y=100)
frame2.pack()

tab.add(frame1, text="Gears")
tab.add(frame2, text="Grade")
tab.select(frame1)
tab.enable_traversal()

tab.pack()
root.mainloop()